#include "library.h"

///Déclaration des variablse globales

TOVC *file;     //fichier de données
TnOF *arch;     //fichier d'archive
int NbE1, NbE2, nb_elevs, nb_salle;     //NbE1, NbE2 tailles des Index, Nombre d'eleves par salle, Nombre de salles
char fname1[20];
char fname2[20];
Index_IDcouple IndexID[IndexIDsize];
Index_Moyennecouple IndexM[IndexMoyennesize];


/** \3.a:
 *  Pour construire un fichier de dossiers scolaires par élève, on parcourt séquentiellement le premier fichier de données, pour récupérer, pour chaque élève
    (enregistrement) l'identifiant de l'élève, ainsi que son tableau de notes pour calculer sa moyenne à partir de ce dernier.
 *  On récupère d'abord tous les élèves d'une salle donnée,on calcule leurs moyennes, et on les stocke dans un tableau de dossiers scolaires de la meme taille que le nombre d'eleves par salle
 *  Après avoir récupéré tous les élèves d'une salle,on trie les dossiers scolaire en ordre décroissant selon la moyenne de l'élève et on charge le fichier d'archive par les élèves de la salle
    une fois le tri est terminé. Chaque enregistrement du fichier d'archive TnOF est donc un enregistrement contenant l'identifiant de l'élève ainsi que sa moyenne de l'année.
 *  En parallèle, on contruit un index non dense sur les moyennes (IndexM) qui contiendra, pour chaque salle, la moyenne minimale et maximale, ainsi que l'adresse de la salle dans le fichier d'archive
    pour éviter de parcourir le fichier une autre fois pour le construire.

    La clé primaire: l'identifiant de l'élève
    La clé secondaire: la moyenne de l'élève
    Type du modèle: TnOF

    Remarque: afin de ne pas créer un autre modèle pour archiver les résultats de la simulation, on garde 5 autres champs dans l'enregistrement du fichier d'archive
              Ils ne seront utiles que durant la simulation.
 */

/** \3.b:
 *  i. On choisira un index non dense sur les moyennes (IndexM), comme index secondaire, et un index dense qui contient les identifiants des élèves pour accéder au fichier de données
    On n'accèdera donc au fichier de données que lorsqu'on a réellement besoin de l'information sur cet étudiant (si sa moyenne vérifie la requete à intervalle, par exemple)

    ii. Comme on accède à travers l'index primaire (dense contenant les identifiants des élèves), on ne met à jour que celui là (Insertion d'un élève/Suppression d'un élève).
    Et comme on n'archive qu'une fois toute modification est terminée (On archive à la fin d'une année scolaire), ces mise-à-jour seront prises en considération lors de la création de l'archive, et
    l'index des moyennes en parallèle.

    ***Il est donc impossible de trouver, dans le fichier d'archive le dossier scolaire d'un élève qui a été supprimé durant l'année, ainsi que si sa moyenne représentait une des bornes de l'intervalle
     des moyennes dans la salle ou il se trouvait (s'il avait la moyenne minimale ou maximale) ceci est automatiquement pris en compte lors de la création de l'archive**
 */

 /** \4.a:
 *  Cout de l'archivage: N lectures + M écritures.
    N est le nombre de blocs dans le fichier de données
    M est le nombre de blocs dans le fichier d'archive
 */

///-----------------------------------------------------------------------------------------------------------------------------///
TOVC * ouvrir(char * filename, char mod)
{
    TOVC * i = malloc(sizeof(TOVC));
    if ((mod == 'A') || (mod == 'a')) {                         //ouverture du fichier en mode ancien
        i -> F = fopen(filename, "rb+");
        fread( & (i -> entete), sizeof(Entete), 1, i -> F);     //récuperer l'entete du fichier
    } else
    if ((mod == 'N') || (mod == 'n')) {                         //ouverture du fichier en mode nouveau
        i -> F = fopen(filename, "wb+");
        (i -> entete).adr_dernier_bloc = 0;
        (i -> entete).nbenreg = 0;
        (i -> entete).indice_libre = 0;
        (i -> entete).nbsup = 0;
        fwrite( & (i -> entete), sizeof(Entete), 1, i -> F);  //initialisation de l'entete d'un fichier vide

    } else {
        printf("\n!! mode d'ouverture errone.");
    }
    return i;
}
///---------------------------------------------------------------------------------------------------------------///
void fermer(TOVC * pF) {
    fseek(pF -> F, 0, 0);                                         // se positionner au début du fichier
    fwrite( & (pF -> entete), sizeof(Entete), 1, pF -> F);        //mise-à-jour de l'entete
    fclose(pF -> F);
}
///---------------------------------------------------------------------------------------------------------------///
int entete(TOVC * pF, int i) {
    if (i == 1) return ((pF -> entete).adr_dernier_bloc);       //adresse du dernier bloc
    if (i == 2) return ((pF -> entete).nbenreg);                //nombre de caractères inséré
    if (i == 3) return ((pF -> entete).indice_libre);           //adresse de la première position libre dans le dernier bloc
    if (i == 4) return ((pF -> entete).nbsup);                  //nombre de caracteres supprimés
}
///---------------------------------------------------------------------------------------------------------------///
void aff_entete(TOVC * pF, int i, int val) {
    if (i == 1)(pF -> entete).adr_dernier_bloc = val;
    else if (i == 2)(pF -> entete).nbenreg = val;
    else if (i == 3)(pF -> entete).indice_libre = val;
    else if (i == 4)(pF -> entete).nbsup = val;

}
///---------------------------------------------------------------------------------------------------------------///

void liredir(TOVC * pF, int i, Buffer * buf) {
    if (i <= entete(pF, 1)) {
        fseek(pF -> F, 0, SEEK_SET);
        fseek(pF -> F, sizeof(Entete) + (i - 1) * sizeof(Tbloc), SEEK_SET);
        fread(buf, sizeof(Tbloc), 1, pF -> F);
    }
}
///---------------------------------------------------------------------------------------------------------------///

void ecriredir(TOVC * pF, int i, Buffer buf) {
    if (i <= entete(pF, 1)) {
        fseek(pF -> F, 0, SEEK_SET);
        fseek(pF -> F, sizeof(Entete) + (i - 1) * sizeof(Tbloc), SEEK_SET);
        int y = fwrite( & buf, sizeof(Tbloc), 1, pF -> F);
        fseek(pF -> F, 0, SEEK_SET);
    }

}
///---------------------------------------------------------------------------------------------------------------///

int alloc_bloc(TOVC * pF) {
    aff_entete(pF, 1, entete(pF, 1) + 1); //incrémente le nombre total des blocs
    return entete(pF, 1);
}

///---------------------------------------------------------------------------------------------------------------///

TnOF * ouvrir_TnOF(char * filename, char mod) // mod = 'A' ancien (rb+) || mod = 'N' nouveau (wb+)
{
    TnOF * i = malloc(sizeof(TnOF));
    if ((mod == 'A') || (mod == 'a')) {
        i -> file = fopen(filename, "rb+");
        fread( & (i -> Entete_TnOF), sizeof(Entete_TnOF), 1, i -> file); //récuperer l'entete du fichier
    } else
    if ((mod == 'N') || (mod == 'n')) {
        i -> file = fopen(filename, "wb+");
        (i -> Entete_TnOF).Nbloc = 0;
        (i -> Entete_TnOF).Nenreg = 0;
        fwrite( & (i -> Entete_TnOF), sizeof(Entete_TnOF), 1, i -> file); //initialisation de l'entete d'un fichier vide
    } else {
        printf("\n!! mode d'ouverture errone.");
    }
    return i;
}
///---------------------------------------------------------------------------------------------------------------///

void fermer_TnOF(TnOF * f) {
    fseek(f -> file, 0, 0); // se positionner au début du fichier
    fwrite( & (f -> Entete_TnOF), sizeof(Entete_TnOF), 1, f -> file); //mise-à-jour de l'entete
    fclose(f -> file);
}

int entete_TnOF(TnOF * f, int i) {
    if (i == 1) return ((f -> Entete_TnOF).Nbloc);
    if (i == 2) return ((f -> Entete_TnOF).Nenreg);
}
///---------------------------------------------------------------------------------------------------------------///

void aff_entete_TnOF(TnOF * f, int i, int val) {
    if (i == 1)(f -> Entete_TnOF).Nbloc = val;
    else if (i == 2)(f -> Entete_TnOF).Nenreg = val;
}
///---------------------------------------------------------------------------------------------------------------///

void liredir_TnOF(TnOF * pF, int i, Buffer_TnOF * buff1) {

    fseek(pF -> file, 0, SEEK_SET);
    fseek(pF -> file, sizeof(Entete_TnOF) + (i - 1) * sizeof(Tbloc_TnOF), SEEK_SET);
    fread(buff1, sizeof(Tbloc_TnOF), 1, pF -> file);
}
///---------------------------------------------------------------------------------------------------------------///

void ecriredir_TnOF(TnOF * pF, int i, Buffer_TnOF buff1) {

    fseek(pF -> file, 0, SEEK_SET);
    fseek(pF -> file, sizeof(Entete_TnOF) + (i - 1) * sizeof(Tbloc_TnOF), SEEK_SET);
    int y = fwrite( & buff1, sizeof(Tbloc_TnOF), 1, pF -> file);
    fseek(pF -> file, 0, SEEK_SET);

}
///---------------------------------------------------------------------------------------------------------------///

int alloc_bloc_TnOF(TnOF * pF) {
    aff_entete_TnOF(pF, 1, entete_TnOF(pF, 1) + 1); //incrémente le nombre total des blocs
    return entete_TnOF(pF, 1);
}
///---------------------------------------------------------------------------------------------------------------///

void RechTnOF(TnOF * arch, char id_elev[5], int * trouv, int * i, int * j) {
    ///Recherche Séquentielle dans le fichier d'archive
    arch = ouvrir_TnOF(fname2, 'A');
    * i = 1;                                 //numéro du 1er bloc
    * j = 0;                                 //premiere position
    * trouv = 0;                                //au début de la recherche, la donnée n'est pas encore trouvée
    int stop = 0;                                //variable pour parcourir le fichier
    liredir_TnOF(arch, * i, & buff1);           //lecture du 1er bloc
    while (stop == 0 && * trouv == 0) {         //tant qu'on n'est pas arrivé en fin de fichier et on n'a pas encore trouvé la donnée
        if (strcmp(buff1.tab[ * j].id_eleve, id_elev) == 0) {
            * trouv = 1;
        } else                                                              ///passer au prochain enregistrment
        {
            if (( * j) < buff1.Nb) {           //on n'a pas parcourru tout le bloc
                ( * j) ++;                     //passer au prochain  enregistrement
            } else {                           //on a parcourru tout le bloc
                ( * i) ++;                     //passer au prochain bloc
                liredir_TnOF(arch, * i, & buff1);               //lecture du prochain bloc
                * j = 0;                                        //premiere position
            }
        }
        if (( * i) == entete_TnOF(arch, 1) && ( * j) >= entete_TnOF(arch, 2))   //on a parcourru tout le fichier sans trouver la donnée
            stop = 1;                                                           //arreter la recherche
    }
    if ( * trouv == 0)                                                      ///retourner la fin du fichier
    {
        ( * i) = entete_TnOF(arch, 1);
        ( * j) = entete_TnOF(arch, 2);
    }
    fermer_TnOF(arch);
}

///---------------------------------------------------------------------------------------------------------------///
///procédure d'insertion dans un fichier TnOF
void InsertionTnOF(TnOF * arch, DossierScolaire info) ///Insertion en fin de fichier
{
    arch = ouvrir_TnOF(fname2, 'A');
    int i = entete_TnOF(arch, 1);           //numéro du dernier bloc
    int j = entete_TnOF(arch, 2);           //dernier enregistrement dans le dernier bloc
    liredir_TnOF(arch, i, & buff1);         //lecture du dernier bloc
    if (buff1.Nb < TnOF_size) {             //tant qu'on n'a pas parcourru tout le bloc
        buff1.tab[j] = info;
        buff1.Nb = buff1.Nb + 1;
    } else {                                //si tout le bloc a été parcourru
        i = alloc_bloc_TnOF(arch);
        j = 0;
        buff1.tab[j] = info;
        buff1.Nb = 1;
    }
    ecriredir_TnOF(arch, i, buff1);        //ecriture du dernier bloc
    if (i != entete_TnOF(arch, 1))          //mise-à-jours de l'entete
        aff_entete_TnOF(arch, 1, i);
    aff_entete_TnOF(arch, 2, entete_TnOF(arch, 2) + 1);
    fermer_TnOF(arch);
}

///---------------------------------------------------------------------------------------------------------------///
///fonction qui genère aléatoirement l'id de la classe
void gen_id_class(char * id_class) {
    char string[7];
    strcpy(string, "P12345");             ///chaine contenant les 6 niveaux
    time_t t;
    srand((unsigned) time( & t));
    int i = rand() % 6;
    id_class[0] = string[i];             ///le premier caractère contient le niveau
    char salles[5];
    char j_char[2];
    int j = 1;
    strcpy(salles, "");
    while (j <= nb_salle) {               ///concaténer les numéros des salles qui existent
        sprintf(j_char, "%1d", j);
        strcat(salles, j_char);
        j++;
    }

    srand((unsigned) time( & t));       //choisir aléatoirement un de ces numéros
    j = rand() % strlen(salles);
    id_class[1] = string[j];
    id_class[2] = '\0';
}
///---------------------------------------------------------------------------------------------------------------///
void gen_nom_gprenom(char nom[], int BI, int BS) {
    FILE * fn = fopen("noms.txt", "r");     //ouverture des fichiers textes
    FILE * fp = fopen("names.txt", "r");
    rewind(fn);
    rewind(fp);
    char tmp_nom[20] = "";                  //initialisation des chaines tempons à vide
    char tmp_prenom[20] = "";
    char prenom_extr[20] = "";
    int alea_indice;
    int i = 1;
    srand(getpid());
    time(NULL);
    alea_indice = (rand() % (BS + BI));         //génération d'un nombre aléatoire
    sprintf(nom, "");
    while (i <= alea_indice)                     //récupérer le nom et le prenom correspondants depuis le fichier des noms et des prenoms
    {
        fscanf(fn, "%[^,]", tmp_nom);
        fseek(fn, 1, SEEK_CUR);
        fscanf(fp, "%[^;]", tmp_prenom);
        fseek(fp, 1, SEEK_CUR);
        i++;
    }

    tmp_nom[strlen(tmp_nom)] = '\0';                    //ecriture du caractere de fin de chaine dans les noms/prenoms récupérés
    tmp_prenom[strlen(tmp_prenom)] = '\0';
    strcpy(nom, tmp_nom);
    strcat(nom, ".");
    strcat(nom, tmp_prenom);
    fclose(fn);
    fclose(fp);
}
///---------------------------------------------------------------------------------------------------------------///
void gen_notes(char id_class[4], char chaine_notes[23], int BI) {
    FILE * fm = fopen("matieres.txt", "r");
    rewind(fm);
    char mark[3] = "", tmp[100] = "";
    strcpy(chaine_notes, "");
    int alea;
    for (int j = 0; j < 11; j++) {
        fscanf(fm, "%[^:]", tmp);               //sauter le nom de la matiere
        fseek(fm, 1, SEEK_CUR);
        alea = (rand() % (20 - (BI - 1) + 1));  //generer aleatoirement un nombre
        fseek(fm, 3 * alea, SEEK_CUR);
        fscanf(fm, "%[^.]", mark);              //recuperer la note relative au nombre depuis le fichier (si alea = 3, on recupere 03)
        if ((id_class[0] == 'P' && (j == 0 || j == 2 || j == 5 || j == 6 || j == 8 || j == 9)) || (id_class[0] == '1' || id_class[0] == '2') && (j == 6)) //S'il s'agit d'un eleve dans la classe préparatoire
        {
            strcpy(mark, "XX");
        }
        mark[2] = '\0';
        strcat(chaine_notes, mark);
        fscanf(fm, "%[^/]", tmp);       //sauter jusqu'à lamatière prochaine
        fseek(fm, 1, SEEK_CUR);
        strcpy(tmp, "");
    }
    chaine_notes[23] = '\0';           //caractère de fin de chaine pour manipuler le tableau des notes
    fclose(fm);                        //fermeture du fichier des matieres
}
///---------------------------------------------------------------------------------------------------------------///
void AleaDonnees(int nb_elevs, char * tab_id_elev[600], char * tab_nom[600]) {
    char * nom = malloc(sizeof(char) * 50);
    char char_id[5];
    int bi1 = 1, bs1 = nb_elevs, n, i = 0, tab_id[9999];
    while (i <= 9999) {                     ///charger le tableau des identifiants uniques 0000-9999
        tab_id[i] = malloc(sizeof(int));      //allocation dynamique de la mémoire
        tab_id[i] = 0;                        //au début, tous les ids sont disponibles
        i++;
    }
    i = 0;
    while (i < nb_elevs) {                  //pour chaque eleve
        srand(getpid());
        time(NULL);
        gen_nom_gprenom(nom, bi1, bs1);     //générer aleatoirement un nom,prenom, ainsi que le genre
        nom[strlen(nom)] = '\0';            //caractère de fin de chaine pour manipuler le nom
        bi1 += 2;
        bs1 -= 6;
        if (bs1 < 300) {                    //mise-à-jour des bornes pour garantir que l'enregistrement soit aléatoir
            bi1 = 1;
            bs1 = 3 * bs1;
        }
        tab_nom[i] = malloc(sizeof(char) * 50);     //stocker les noms générés dans un tableau afin de les ordonner alphabétiquement plus tard
        sprintf(tab_nom[i], nom);
        i++;
    }
    i = 0;
    while (i < nb_elevs) {                      //pour chaque eleve
        srand(getpid());
        time(NULL);
        n = rand() % 9999;                      //generer aleatoirement un identifiant
        while (tab_id[n] == 1) {                  //si cet identifiant est déja pris, on met la case dont il est l'indice à 1
            time(NULL);
            n = rand() % 9999;                     //generer un deuxieme identifiant jusqu'a avoir un qui n'est pas déjà pris
        }
        tab_id[n] = 1;
        sprintf(char_id, "%04d", n);            //convertir l'identifiant en une chaine sur 4 caracteres
        char_id[5] = '\0';
        tab_id_elev[i] = malloc(sizeof(char) * 5);      //stocker l'id dans le tableau des identiiants générés
        sprintf(tab_id_elev[i], char_id);
        sprintf(char_id, "");
        sprintf(nom, "");
        i++;
    }
}
///---------------------------------------------------------------------------------------------------------------///
//récupère une chaine de 'nbcar' caractères à partir de la position j du bloc i du fichier et la sauvegarde dans buff_chaine
void recup_chaine(TOVC * file, int * i, int * j, int nbcar, char buff_chaine[]) {
    int k = 0;
    Buffer buff;
    liredir(file, * i, & buff);                      //lecture du bloc i dans le buffer
    while ((k < nbcar))                              //tant que la taille de la chaine récupérée est < au 'nbcar'
    {
        if (( * j) < TOVC_size)                     //la taille du bloc i n'est pas dépassée
        {

            buff_chaine[k] = buff.chaine[ * j];             //copier le caractère j dans la position k de buff_chaine
            k++;
            ( * j) = ( * j) + 1;                        //passer au caractère suivant dans le bloc

        } else {

            ( * j) = 0;
            ( * i) = ( * i) + 1;                            //numéro du prochain bloc à lire
            liredir(file, * i, & buff);                     //lecture du prochain bloc
        }
    }
    buff_chaine[k] = '\0';                                   //écriture du caractère de fin de chaine
}

//-----------------------------------------------------------------------------------
//écrit une chaine 'buff_chaine' à partir de la position j dans le bloc i
void ecrire_chaine(TOVC * file, int * i, int * j, int nbcar, char buff_chaine[]) {
    int k = 0;
    Buffer buff;
    liredir(file, * i, & buff);
    while (k < nbcar)                                   //il reste des caractères dans la chaine 'buff_chaine' à écrire
    {
        if (( * j) < TOVC_size)                         // si la taille du bloc i n'est pas encore dépassée
        {
            buff.chaine[ * j] = buff_chaine[k];         //écrire le jième caractère à la position k dans buff.tab[j]
            k++;
            ( * j) = ( * j) + 1;                        //passer au caractère suivant
        } else                                           //cas du chevauchement: la taille du bloc est dépassée
        {
            ecriredir(file, ( * i), buff);
            ( * i) = alloc_bloc(file);
            liredir(file, * i, & buff);
            * j = 0;
        }
    }
    buff.chaine[strlen(buff.chaine)] = '\0';            //caractère de fin de chaine
    ecriredir(file, ( * i), buff);
}
///---------------------------------------------------------------------------------------------------------------///
//procédure de la recherche dans un fichier TOVC
void RechTOVC(TOVC * file, char fname1[], char RechID[3], char RechNom[40], int * trouv, int * i, int * j) {
    char tmp_taille[4], tmp_eff[2], tmp_elev[5], tmp_id[3], tmp_gn[2], tmp_notes[23], tmp_RechID[3]; ///Variables tempons pour extraire les champs de l'enregistrement
    char * tmp_nom = malloc(sizeof(char) * 30);
    int stop = 0, tmp_i, tmp_j;
    file = ouvrir(fname1, 'A');
    * i = 1;* j = 0;* trouv = 0;
    int int_taille;
    if (entete(file, 1) != 0)                            ///Recherche séquentielle
    {
        while (( * trouv == 0) && (stop == 0) && ( * i <= entete(file, 1))) {
            tmp_i = * i;                              ///sauvegarde des indices avant de la récuperation de l'enregistrement
            tmp_j = * j;
            recup_chaine(file, & ( * i), & ( * j), 3, tmp_taille);
            int_taille = atoi(tmp_taille);
            recup_chaine(file, & ( * i), & ( * j), 1, tmp_eff);
            recup_chaine(file, & ( * i), & ( * j), 4, tmp_elev);
            recup_chaine(file, & ( * i), & ( * j), 2, tmp_id);
            recup_chaine(file, & ( * i), & ( * j), int_taille + 1, tmp_nom);
            recup_chaine(file, & ( * i), & ( * j), 1, tmp_gn);
            recup_chaine(file, & ( * i), & ( * j), 22, tmp_notes);
            if (( * i == entete(file, 1)) && ( * j > entete(file, 3)))   //Si on a dépassé l'entete du fichier, on sort de la boucle
            {
                * i = entete(file, 1);
                * j = entete(file, 3);
                stop = 1;
            } else {
                strcpy(tmp_RechID, RechID);
                if (tmp_RechID[0] == 'P') {             //Si l'id recherché/tempon de la classe contient P, on remplace par '0' pour pouvoir comparer
                    tmp_RechID[0] = '0';                //entre les codes ASCIIs des 2 identifiants.
                }
                if (tmp_id[0] == 'P') {
                    tmp_id[0] = '0';
                }
                if ((strcmp(tmp_RechID, tmp_id) == 0) && (strcmp(tmp_nom, RechNom) == 0) && (strcmp(tmp_eff, "0") == 0))  { //Cas ou on trouve la donnee
                    * trouv = 1;
                    * i = tmp_i;
                    * j = tmp_j;
                } else                                 //Si ce n'est pas ce qu'on cherche, on cherche l'emplacemet là ou on doit l'insérer
                {

                    if (strcmp(tmp_id, tmp_RechID) > 0)         //Si l'ID de la classe actuel est supérieur à celui que l'on cherche.
                    {
                        stop = 1;
                        * i = tmp_i;
                        * j = tmp_j;
                    } else                                      // si l'ID de la classe est égal à celui que l'on cherche, on vérifie le nom
                    {
                        if (strcmp(tmp_id, tmp_RechID) == 0) {

                            if (strcmp(tmp_nom, RechNom) > 0) {     //on s'arrete dès qu'on retrouve un nom qui vient juste après celui que l'on cherche dans l'ordre alphabétique
                                stop = 1;
                                * i = tmp_i;
                                * j = tmp_j;
                            }
                        }
                    }

                }
            }
        }
    }

}
///---------------------------------------------------------------------------------------------------------------///
//procédure de la suppression logique dans un fichier TOVC

void SuppLogiqueTOVC(TOVC * file, char fname1[], char nom[], char id_class[3]) {
    file = ouvrir(fname1, 'A');
    int i, j, trouv, tmp_i, tmp_j;
    char taille[3];
    RechTOVC(file, fname1, id_class, nom, & trouv, & i, & j);               //rechercher l'information
    file = ouvrir(fname1, 'A');
    tmp_i = i;
    tmp_j = j;
    if (trouv == 1)                                                         //si l'enregistrement existe, on positionne son champs effacé à 1
    {
        printf("\n\nInformations Eleve:\n\n");
        AffichEnreg(file, fname1, i, j);
        recup_chaine(file, & i, & j, 3, taille);                            //récuperer la taille du champs variable
        ecrire_chaine(file, & i, & j, 1, "1");                              //Mettre le champs effacé à 1
        aff_entete(file, 4, atoi(taille) + 34);                             //Mettre-à-jour le nombre de caractères supprimés
        fermer(file);
        printf("\nSuppression avec succes.");
    } else
        printf("\n\nELEVE INEXISTANT.\n\n");

}
///---------------------------------------------------------------------------------------------------------------///
//procédure du chargement initial du fichier de données

void ChgmtInit(TOVC * file, char fname[], int nb_elevs, int nb_salle) {
    ///l,k pour parcourir le fichier à creer ; salle_elevs est le nombre des eleves dans chaque salle, en supposant que toutes
    ///les salles sont remplies.
    int i = 0, l = 0, somme = 0, bi3 = 1, taille, salle_elevs = (nb_elevs / 6) / nb_salle;

    char tmp_nom[50], chaine[30] = "", tai[4], id_class[3], niveau_char[2], salle_char[2], NomPrenom[max_info], info[max_info], temp[50]; ///variables pour generer l'information
    ///tableaux des donnees generes aleatoirement
    char * tab_id_elev[nb_elevs];
    char * tab_nom[nb_elevs];
    char * tab_nom_tri[nb_elevs];
    char * tab_id[600];
    ///Generer le tableau des identifiants uniques ainsi que les noms/prenoms
    AleaDonnees(nb_elevs, tab_id_elev, tab_nom);
    file = ouvrir(fname, 'N');                                  ///Creation d'un fichier vide
    int k = alloc_bloc(file);
    fermer(file);

    file = ouvrir(fname, 'A');
    i = 0;
    for (int niveau = 0; niveau <= 5; niveau++) {               //pour les 6 niveaux
        if (niveau == 0)
            strcpy(id_class, "P");
        else {
            sprintf(niveau_char, "%d", niveau);
            id_class[0] = niveau_char[0];
        }
        for (int salle = 1; salle <= nb_salle; salle++) {       //pour chaque classe dans le niveau
            sprintf(salle_char, "%d", salle);                   //Construire l'identifiant de la classe
            id_class[1] = salle_char[0];
            id_class[2] = '\0';
            int sauv_i = i;
            int tmp_i = i;
            for (i = sauv_i; i < salle_elevs + sauv_i; i++) {
                for (int j = i + 1; j < salle_elevs + sauv_i; j++)      //trier les eleves de la salle en ordre alphabétique
                {
                    if (strcmp(tab_nom[i], tab_nom[j]) > 0) {
                        strcpy(temp, tab_nom[i]);
                        strcpy(tab_nom[i], tab_nom[j]);
                        strcpy(tab_nom[j], temp);
                    }
                }
            }
            for (int m = 0; m < salle_elevs; m++) {
                tab_nom_tri[m] = malloc(sizeof(char) * 50);              //sauvegarder les noms triés dans un tableau
                sprintf(tab_nom_tri[m], tab_nom[sauv_i]);
                sauv_i++;
            }

            for (int m = 0; m < salle_elevs; m++) {                     //pour chaque nom, génerer aleatoirement les autres champs
                strcpy(info, "");
                taille = strlen(tab_nom_tri[m]) - 2;
                sprintf(tai, "%03d", taille);
                strcat(info, tai);
                strcat(info, "0");
                strcat(info, tab_id_elev[tmp_i]);
                strcat(info, id_class);
                strcat(info, tab_nom_tri[m]);
                strcpy(chaine, "");
                gen_notes(id_class, chaine, bi3);
                strcat(info, chaine);
                ecrire_chaine(file, & k, & l, strlen(info), info);      //ecriture de l'information dans le fichier
                tmp_i++;
                somme += strlen(info);                                  //somme contient le nombre de caracteres inseres
            }
        }

    }
    aff_entete(file, 1, k);                                           //mise-à-jour de l'entete
    aff_entete(file, 2, somme);
    aff_entete(file, 3, l);
    fermer(file);
    printf("\n\n\t>> Fin du chargement initial du fichier.");
    file = ouvrir(fname1, 'A');
    AffichTOVC(fname1);                                                             //Affichage du fichier
}
///---------------------------------------------------------------------------------------------------------------///
//procédure de l'insertion dans un fichier TOVC
void InsertionTOVC(TOVC* file,char fname1[],char info[],char id_class[], char NomPrenom[])
{
    file = ouvrir(fname1,'a');
    int i,j,trouv,FIN,indc,o,nb,ind,var,stop = 0 ;
    char x;
    RechTOVC(file,fname1,id_class,NomPrenom,&trouv,&i,&j);
     indc = 0 ;
    if (trouv == 0)
    {
         if ( (i == entete(file,1)) && (j==entete(file,3)))
             {
                 ecrire_chaine(file,&i,&j,strlen(info),info);
             }
            else
            {
                char *tab = malloc(sizeof(char)*(strlen(info))); ///tableau de recopie contenant tous les caracteres de info
                ///Copier l'enregistrement dans le tableau de recopie
                o = 0 ;
                while( o < strlen(info))
                {
                    tab[o] = info[o];
                    o++;
                }
                while ( i <= entete(file,1) && (stop == 0))     //tant qu'on n'a pas parcourru tout le fichier
                {
                    if (i == entete(file,1))                    //si on est dans le dernier bloc, on décale jusqu'au premier indice libre
                        nb = entete(file,3);
                    else
                        nb = TOVC_size;                         //sinon on décale jusqu'a ce que le bloc soit terminé
                    liredir(file,i,&buff);
                    while(j<nb)                                 //recuperer les caracteres dans un tableau tant que j!=nb
                    {
                        x = buff.chaine[j];                     //x contiendra le caractere récupéré depuis le fichier
                        buff.chaine[j] = tab[indc];
                        tab[indc] = x ;
                        j++ ;
                        indc = (indc + 1 ) % strlen(info);      //incrementation du ind modulo la taille de l'enregistrement à inserer
                    }
                    if (j==TOVC_size)
                        j = 0 ;
                    if (i != entete(file,1))                    //si i != du dernier bloc
                    {
                        ecriredir(file,i,buff);                 //ecrire le bloc
                        i++ ;                                   //passer au suivant
                    }

            ind = indc;
            indc -= 1   ;
            if(indc < 0)                                        //indc est incrémeté modulo la taille de l'enregistrement
            {
                indc = strlen(info);
            }
            while(ind != indc )
            {
                    buff.chaine[j] = tab[ind];
                    ind = (ind+1) % strlen(info);       //incrementer ind modulo la taille de l'enregistrement
                    j++ ;

                if (j==TOVC_size)               //si le bloc est entierement parcourru, le mettre a jour et passer au suivant
                {
                    ecriredir(file,i,buff);
                    j=0;
                    i++;
                }
            }
                buff.chaine[j] = tab[ind];
                j++;                                                    //si le j dépasse l'entete,
             /*   if (strlen(info)+entete(file,3)>TOVC_size)
                    var = strlen(info) + entete(file,3) - TOVC_size ;
                else */
                    var = strlen(info) + entete(file,3);
                if (j==var)
                    stop = 1 ;
                ecriredir(file,i,buff);
                }
        }

    }
    aff_entete(file,1,i);
    aff_entete(file,3,j);
    fermer(file);
}

///---------------------------------------------------------------------------------------------------------------///
//procédure d'affichage d'un fichier TOVC

void AffichTOVC(char fname[]) {
    int i = 1, j = 0,int_taille,count = 1;                //variables tempons pour récuperer les champs
    char tmp_taille[4] = "", tmp_eff[2] = "", tmp_elev[5] = "", save_elev[5] = "", tmp_id[3] = "", save_id[3] = "", tmp_gn[8] = "", tmp_notes[23] = "", tmp_RechID[3] = "";
    char * nom = malloc(sizeof(char) * 20);                 //allocation memoire des chaines de taille variable
    char * prenom = malloc(sizeof(char) * 20);
    char * tmp_nom = malloc(sizeof(char) * 30);
    TOVC * file = ouvrir(fname, 'a');
    printf("ENTETE : %d\t%d\t%d\t%d\n", entete(file, 1), entete(file, 2), entete(file, 3), entete(file, 4));
    while ((i < entete(file, 1)) || ((i == entete(file, 1)) && (j < entete(file, 3)))) {    //parcours séquentiel du fichier
        recup_chaine(file, & i, & j, 3, tmp_taille);
        int_taille = atoi(tmp_taille);
        recup_chaine(file, & i, & j, 1, tmp_eff);                                           //récuperer les différents champs
        recup_chaine(file, & i, & j, 4, tmp_elev);
        recup_chaine(file, & i, & j, 2, tmp_id);
        recup_chaine(file, & i, & j, int_taille + 1, tmp_nom);
        recup_chaine(file, & i, & j, 1, tmp_gn);
        recup_chaine(file, & i, & j, 22, tmp_notes);
        printf("\n\n - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");
        if (strcmp(tmp_gn, "M") == 0)
            strcpy(tmp_gn, "Garcon");
        else
            strcpy(tmp_gn, "Fille");
        if (strcmp(tmp_eff, "0") == 0)                          //si l'enregistrement n'est pas effacé, on affiche ses champs
        {
            Split_Nom_Prenom(tmp_nom, nom, prenom);
            printf("\n**%d**\nID Eleve:\t%s\n\nID Classe:\t%s\n\nNom:\t\t%s\t\tPrenom:\t\t%s\n\nGenre:\t\t%s\n\nNotes:\t\t%s",count, tmp_elev, tmp_id, nom, prenom, tmp_gn, tmp_notes);
            printf("\n\n - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - \n\n");
            count++ ;
        }

    }
    fermer(file);
}
///---------------------------------------------------------------------------------------------------------------///
//procédure d'affichage d'un enregistrement dans le fichier de données

AffichEnreg(TOVC * file, char fname[], int i, int j) {
    int int_taille;
    //variables tempons pour récuperer es champs
    char tmp_taille[4] = "", tmp_eff[2] = "", tmp_elev[5] = "", save_elev[5] = "", tmp_id[3] = "", save_id[3] = "", tmp_gn[8] = "", tmp_notes[23] = "", tmp_RechID[3] = "";
    char * nom = malloc(sizeof(char) * 20);                 //allocation memoire des chaines de taille variable
    char * prenom = malloc(sizeof(char) * 20);
    char * tmp_nom = malloc(sizeof(char) * 30);
    file = ouvrir(fname, 'A');
    recup_chaine(file, & i, & j, 3, tmp_taille);                //récuperer les champs de l'enregistrement à partir de la position i,j
    int_taille = atoi(tmp_taille);
    recup_chaine(file, & i, & j, 1, tmp_eff);
    recup_chaine(file, & i, & j, 4, tmp_elev);
    recup_chaine(file, & i, & j, 2, tmp_id);
    recup_chaine(file, & i, & j, int_taille + 1, tmp_nom);
    recup_chaine(file, & i, & j, 1, tmp_gn);
    recup_chaine(file, & i, & j, 22, tmp_notes);
    printf("\n\n - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");
    if (strcmp(tmp_gn, "M") == 0)
        strcpy(tmp_gn, "Garcon");
    else
        strcpy(tmp_gn, "Fille"); {
        Split_Nom_Prenom(tmp_nom, nom, prenom);
        printf("\nID Eleve:\t%s\n\nID Classe:\t%s\n\nNom:\t\t%s\t\tPrenom:\t\t%s\n\nGenre:\t\t%s\n\nNotes:\t\t%s", tmp_elev, tmp_id, nom, prenom, tmp_gn, tmp_notes);
        printf("\n\n - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - \n\n");

    }
    fermer(file);
}

///---------------------------------------------------------------------------------------------------------------///
//Procédure d'affichage de la table d'index des moyennes
void AffichIndexM(Index_Moyennecouple IndexM[IndexMoyennesize], int NbE) {
    for (int i = 0; i < NbE; i++) {
        printf("\nSalle: %s\tMoyenne Min: %.2f\tMoyenne Max: %.2f\tBloc: %d\tPosition: %d", IndexM[i].salle, IndexM[i].Min, IndexM[i].Max, IndexM[i].bloc, IndexM[i].pos);
    }
}
///---------------------------------------------------------------------------------------------------------------///
//Procédure d'affichage de la table d'index des identifiants
void AffichIndexID(Index_IDcouple IndexID[IndexIDsize], int NbE) {
    for (int i = 0; i < NbE; i++) {
        if (IndexID[i].eff == 0)                            //Si l'eleve n'est pas supprimé du fichier de données
            printf("\nIdentifiant: %s\tAdresse Bloc: %d\tPosition: %d", IndexID[i].id_elev, IndexID[i].i, IndexID[i].j);
    }
}

///---------------------------------------------------------------------------------------------------------------///
//Procédure qui coupe NomPrenom en 2 parties Nom et Prenom en fonction du délimiteur '.'
void Split_Nom_Prenom(char NomPrenom[40], char Nom[20], char Prenom[15]) {
    int i = 0;
    while (NomPrenom[i] != '.') {   //tant que le caractere est != du delimiteur
        Nom[i] = NomPrenom[i];      //recuperer le nom
        i++;
    }
    Nom[i] = '\0';
    i = strlen(Nom) + 1;
    int j = 0;
    while (NomPrenom[i] != '\0') {
        Prenom[j] = NomPrenom[i];           //recuperer ce qui reste de la chaine NomPrenom dans le prenom (qui contiendra aussi le genre)
        i++;
        j++;
    }
    Prenom[strlen(NomPrenom) - strlen(Nom) - 1] = '\0';     //insertion du caractere de fin de chaine pour la manipuler
}

///---------------------------------------------------------------------------------------------------------------///
//Procédure de mise-à-jour de la salle d'un eleve 'NomPrenom'
void MAJ_Salle(TOVC * file, char fname1[], char NomPrenom[], char Ancien_id[3], char salle) {
    file = ouvrir(fname1, 'A');
    int i, j, trouv, tmp, tmp_i, tmp_j;
    char Nouveau_id[2], taille_char[3], AncienInfo[max_info], NouvInfo[max_info];
    RechTOVC(file, fname1, Ancien_id, NomPrenom, & trouv, & i, & j);
    tmp_i = i;
    tmp_j = j;
    if (trouv == 1)                                        //Si l'élève en question est inscrit
    {
        printf("\n\n\n**Informations Eleve**\n\n");
        AffichEnreg(file, fname1, i, j);
        recup_chaine(file, & i, & j, 3, taille_char);        //Récuperer l'enregistrement
        recup_chaine(file, & tmp_i, & tmp_j, 34 + atoi(taille_char), AncienInfo);
        AncienInfo[9] = salle;                               //Mettre-à-jour la salle
        strcpy(NouvInfo, AncienInfo);
        strcpy(Nouveau_id, Ancien_id);
        Nouveau_id[1] = salle;
        SuppLogiqueTOVC(file, fname1, NomPrenom, Ancien_id);                //Le supprimer de l'ancien emplacement
        InsertionTOVC(file, fname1, NouvInfo, Nouveau_id, NomPrenom);       //L'insérer dans le nouvel emplacement pour garder le fichier ordonné
        printf("\n\n\n***Mise-a-jour effectuee avec succes***\n\n\n");
        RechTOVC(file, fname1, Nouveau_id, NomPrenom, & trouv, & i, & j);
        AffichEnreg(file, fname1, i, j);
    } else
        printf("\nELEVE INEXISTANT !!! \n");
}

///---------------------------------------------------------------------------------------------------------------///
//Procédure de mise-à-jour d'une des notes de NomPrenom

void MAJ_note(TOVC * file, char fname[], char NomPrenom[], char id_class[], int matiere, char note[]) {
    file = ouvrir(fname, 'A');
    int i, j, trouv, tmp_i, tmp_j, sauv_i, sauv_j;
    char taille[3], tmp[max_info], tmp_notes[23], Nouv_notes[23];
    RechTOVC(file, fname, id_class, NomPrenom, & trouv, & i, & j);
    sauv_i = i;
    sauv_j = j;
    if (trouv == 1)                                     //Si l'eleve recherché est inscrit
    {
        printf("\n**Informations Eleve**\n");
        AffichEnreg(file, fname, i, j);
        recup_chaine(file, & i, & j, 3, taille);
        recup_chaine(file, & i, & j, atoi(taille) + 9, tmp);    //se positionner au niveau du tableau des notes
        tmp_i = i;
        tmp_j = j;                                              //sauvegarder la position correspondante au début du tableau des notes
        recup_chaine(file, & i, & j, 22, tmp_notes);
        printf("\n**Notes Eleve**\n");
        AffichNotes(tmp_notes);
        strcpy(Nouv_notes, tmp_notes);
        Nouv_notes[2 * matiere - 1] = note[0];
        Nouv_notes[2 * matiere] = note[1];
        tmp_j -= 1;
        ecrire_chaine(file, & tmp_i, & tmp_j, 22, Nouv_notes);
        printf("\n**Mise a jour effectuee avec succes**\n");
        AffichEnreg(file, fname, sauv_i, sauv_j);
    } else
        printf("\n Eleve inexistant!!");
}

///---------------------------------------------------------------------------------------------------------------///
//Procédure qui affiche les notes d'un eleve en se basant sur son tableau de notes
void AffichNotes(char chaine_notes[23]) {
    printf("\n\n - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ");
    char matiere[20], note[2];
    int i = 0;
    while (i < 22) {
        if (i == 0) strcpy(matiere, "1. Anglais");
        if (i == 2) strcpy(matiere, "2. Arabe");
        if (i == 4) strcpy(matiere, "3. Dessin");
        if (i == 6) strcpy(matiere, "4. E/Civique");
        if (i == 8) strcpy(matiere, "5. EIslamique");
        if (i == 10) strcpy(matiere, "6. Francais");
        if (i == 12) strcpy(matiere, "7. Hist/Geo");
        if (i == 14) strcpy(matiere, "8. Maths");
        if (i == 16) strcpy(matiere, "9. Musique");
        if (i == 18) strcpy(matiere, "10. Tamazight");
        if (i == 20) strcpy(matiere, "11. Technologie");
        matiere[strlen(matiere)] = '\0';
        printf("\n* %s *:\t\t", matiere);
        note[0] = chaine_notes[i];
        i++;
        note[1] = chaine_notes[i];
        note[2] = '\0';
        printf("%s/20", note);
        i++;
    }
    printf("\n\n - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ");
}

///procedure de creation d'un index dense sur l'identifiant unique de l'eleve
/** \Description:
 *  IndexID est un index dense contenant les identifiants des eleves ; il servira à acceder directement
 *  au fichier initial plus tard, dans la requete à intervalle par exemple, pour recuperer les informations
 *  sur  l'eleve dont la moyenne répond à cette requete.
 *  De cette façon, IndexID servira comme index primaire; Cet index est mis-à-jour apres les opération d'insertion
 *  et de suppression
    On n'aura donc pas besoin de mettre-à-jour l'index sur les moyennes (non dense)
 */

void CreatIndexID(Index_IDcouple IndexID[IndexIDsize], TOVC * file, char fname[], int * NbE) {
    int i = 1, j = 0, int_taille, tmp_i, tmp_j, ind = 0;
    char tmp_taille[4] = "", tmp_eff[2] = "", tmp_elev[5] = "", tmp_nom[50] = "", save_elev[5] = "", tmp_id[3] = "", save_id[3] = "", tmp_gn[8] = "", tmp_notes[23] = "";
    file = ouvrir(fname, 'A');
    while ((i < entete(file, 1)) || ((i == entete(file, 1)) && (j < entete(file, 3)))) {
        //parcourt séquentiel du fichier et construction de la table Index
        tmp_i = i;
        tmp_j = j;
        recup_chaine(file, & i, & j, 3, tmp_taille);            //Récuperer les champs de l'enregistrment
        int_taille = atoi(tmp_taille);
        recup_chaine(file, & i, & j, 1, tmp_eff);
        recup_chaine(file, & i, & j, 4, tmp_elev);
        recup_chaine(file, & i, & j, 2, tmp_id);
        recup_chaine(file, & i, & j, int_taille + 1, tmp_nom);
        recup_chaine(file, & i, & j, 1, tmp_gn);
        recup_chaine(file, & i, & j, 22, tmp_notes);
        IndexID[ind].eff = atoi(tmp_eff);                       //sauvegarde dans une case de la table
        IndexID[ind].i = tmp_i;
        IndexID[ind].j = tmp_j;
        strcpy(IndexID[ind].id_elev, tmp_elev);
        ind++;
    }
    * NbE = ind;
    NbE1 = ind;
    TriRapideIndexID(IndexID, 0, ind - 1);                      //tri de la table d'Index
    fermer(file);
}


///---------------------------------------------------------------------------------------------------------------///
//Fonction qui retourne la moyenne d'un eleve en fonction de son tableau de notes
float moyenne(char chaine[23]) {

    int i = 0, nb_matieres = 0;                   //initialement le nombre de matiere = 0
    float somme = 0, mark;
    char note[3];
    while (i < 22) {
        strcpy(note, "");
        if (chaine[i] != 'X')                      //Si la note correspond à une matière du niveau
        {
            nb_matieres += 1;
            note[0] = chaine[i];
            i++;
            note[1] = chaine[i];
            note[2] = '\0';
            mark = atof(note);
            i++;
            somme += mark;
        } else
            i = i + 2;                          //Si la matière n'est pas prise en compte, la note est sautée
    }
    return (somme / nb_matieres);
}

///Menu
/**************************************************************************************************************************************/

void bienvenu(){
    printf("\n\n\n\n\n                                ------------------------------------------------------                             \n\n");
    Sleep(100);
    printf("                                Travail Pratique - Structure de fichiers et de donnees                             ");
    Sleep(100);
    printf("\n\n                                ------------------------------------------------------                             \n\n");
    printf("\n                                               Gestion de la scolarite                        ");
    Sleep(100);
    printf("\n\n\n\n\n\n\n\n                                           Realise par: MESSIKH Wissal");
    Sleep(100);
    printf("\n\n\n                                             Encadre par: Mme ARTABAZ");
    Sleep(100);
    printf("\n\n\n\n\n\n\n\n\n");
    system("pause");
}

void MenuPrincipal() {
    system("cls");
    printf("\n\n\n                                                                                     ");
    printf("\n                                                                                                    ");
    printf("\n                                G E S T I O N      D E     L A      S C O L A R I T E                           ");
    printf("\n                                                                                 ");
    printf("\n                                                                                              ");
    printf("\n\n\n             ____________________________________________________________________________________________");
    printf("\n            |  ________________________________________________________________________________________  |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |             ___                                                                        | |");
    printf("\n            | |            |   |                                                                       | |");
    printf("\n            | |            | 1 |        Manipulation du fichier Initial                                | |");
    printf("\n            | |            |___|                                                                       | |");
    printf("\n            | |             ___                                                                        | |");
    printf("\n            | |            |   |                                                                       | |");
    printf("\n            | |            | 2 |        Simulation de 5 ans d'etude / Lister les eleves                | |");
    printf("\n            | |            |___|                                                                       | |");
    printf("\n            | |             ___                                                                        | |");
    printf("\n            | |            |   |                                                                       | |");
    printf("\n            | |            | 3 |        Transfert d'un eleve                                           | |");
    printf("\n            | |            |___|                                                                       | |");
    printf("\n            | |             ___                                                                        | |");
    printf("\n            | |            |   |                                                                       | |");
    printf("\n            | |            | 4 |        Quitter le programme                                           | |");
    printf("\n            | |            |___|                                                                       | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |________________________________________________________________________________________| |");
    printf("\n            |____________________________________________________________________________________________|");
    int choix;
    printf("\n\n\t     >>   Entrez votre choix:\t");
    scanf("%d", & choix);
    switch (choix) {
    case 0: {
        goto end;
    }
    case 1: {
        system("cls");
        menu();
        system("pause");
    }
    case 2: {
        system("cls");
        printf("\n\n\n________________________________________________________________________________________________________________\n");
        printf("\n                                                  SIMULATION                                               ");
        printf("\n_________________________________________________________________________________________________________________\n\n\n");
        printf("\n\nBienvenue dans la simulation de 5 ans d'etude.\nCette ecole vient d'ouvrir ses portes.\nIntroduisez le nombre d'eleves rejoignant l'ecole en son ouverture (100-125):\t");
        strcpy(fname1,"Simulation");
        strcpy(fname2,"ArchivSimulation");
        scanf("%d", & nb_elevs);
        printf("\nIntroduisez le nombre de salle par niveau dont dispose l'ecole:\t");
        scanf("%d", & nb_salle);
        Simulation();
        printf("\n\n                                    Eleves avec +12 de moyenne sur 5 ans                             ");
        printf("\n                                    ------------------------------------                             \n\n");
        Lister();
        system("pause");
        MenuPrincipal();
    }
    case 3: {
        system("cls");
        printf("\n\n\n________________________________________________________________________________________________________________\n");
        printf("\n                                             TRANSFERT D'UN ELEVE                                              ");
        printf("\n_________________________________________________________________________________________________________________\n\n\n");
        Transfert();
        system("pause");
        MenuPrincipal();
    }
    }

    end: return 0;
}

void menu() {
    int choix, trouv,bloc,pos,k,intid_elev;
    char NomPrenom[max_info], matiere, taii[4], chaine[max_info], info[max_info], note[2], char_salle, char_genre[2], fname[10], nom[20], prenom[20], genre[1], id_class[3], id_elev[5], salle[1];
    menuprincipal:
    system("cls");
    printf("\n\n\n                                                                                     ");
    printf("\n                                                                                                    ");
    printf("\n                                G E S T I O N      D E     L A      S C O L A R I T E                           ");
    printf("\n                                                                                 ");
    printf("\n                                                                                              ");
    printf("\n\n\n             ____________________________________________________________________________________________");
    printf("\n            |  ________________________________________________________________________________________  |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |             ___                                                                        | |");
    printf("\n            | |            |   |                                                                       | |");
    printf("\n            | |            | 1 |        Creation d'un nouveau fichier                                  | |");
    printf("\n            | |            |___|                                                                       | |");
    printf("\n            | |             ___                                                                        | |");
    printf("\n            | |            |   |                                                                       | |");
    printf("\n            | |            | 2 |        Visualisation d'un fichier existant                            | |");
    printf("\n            | |            |___|                                                                       | |");
    printf("\n            | |             ___                                                                        | |");
    printf("\n            | |            |   |                                                                       | |");
    printf("\n            | |            | 3 |        Ajout d'un eleve                                               | |");
    printf("\n            | |            |___|                                                                       | |");
    printf("\n            | |             ___                                                                        | |");
    printf("\n            | |            |   |                                                                       | |");
    printf("\n            | |            | 4 |        Suppression d'un eleve                                         | |");
    printf("\n            | |            |___|                                                                       | |");
    printf("\n            | |             ___                                                                        | |");
    printf("\n            | |            |   |                                                                       | |");
    printf("\n            | |            | 5 |        Mise-A-Jour des donnees d'un eleve                             | |");
    printf("\n            | |            |___|                                                                       | |");
    printf("\n            | |             ___                                                                        | |");
    printf("\n            | |            |   |                                                                       | |");
    printf("\n            | |            | 6 |        Archivage des resultats annuels                                | |");
    printf("\n            | |            |___|                                                                       | |");
    printf("\n            | |             ___                                                                        | |");
    printf("\n            | |            |   |                                                                       | |");
    printf("\n            | |            | 7 |        Requete sur les moyennes                                       | |");
    printf("\n            | |            |___|                                                                       | |");
    printf("\n            | |             ___                                                                        | |");
    printf("\n            | |            |   |                                                                       | |");
    printf("\n            | |            | 0 |        Quitter le programme                                           | |");
    printf("\n            | |            |___|                                                                       | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |                                                                                        | |");
    printf("\n            | |________________________________________________________________________________________| |");
    printf("\n            |____________________________________________________________________________________________|");

    printf("\n\n\t     >>   Entrez votre choix:\t");
    scanf("%d", & choix);
    switch (choix) {
    case 0: ///Fin programme
    {
        goto end;
    }
    case 1: ///Creation d'un nouveau fichier
    {
        system("cls");
        printf("\n\n\n________________________________________________________________________________________________________________\n");
        printf("\n                                             CREATION D'UN FICHIER                                    ");
        printf("\n_________________________________________________________________________________________________________________\n\n\n");
        printf("Introduire le nom du fichier de donnees:\t");
        scanf("%s", fname1);
        printf("\nTotal des etudiants de l'ecole:\t");
        scanf("%d", & nb_elevs);
        printf("\nNombre de salles par niveau:\t");
        scanf("%d", & nb_salle);
        ChgmtInit(file, fname1, nb_elevs, nb_salle);            //chargement initial du fichier
        CreatIndexID(IndexID,file,fname1,&NbE1);
        system("pause");
        goto menuprincipal;
        case 2: {
            system("cls");
            printf("\n\n\n________________________________________________________________________________________________________________\n");
            printf("\n                                                 OUVERTURE FICHIER                                    ");
            printf("\n_________________________________________________________________________________________________________________\n\n\n");
            printf("\n\n          Entrez le nom du fichier a visualiser: ");
            scanf("%s", fname);
            file = ouvrir(fname1, 'A');
            AffichTOVC(fname);
            file = ouvrir(fname1, 'A');
            system("pause");
            goto menuprincipal;
        }
        case 3: {

            system("cls");
            printf("\n\n\n________________________________________________________________________________________________________________\n");
            printf("\n                                                 AJOUT D'UN ELEVE                                    ");
            printf("\n_________________________________________________________________________________________________________________\n\n\n");
            printf("\n\n          >>  Entrez les informations concernant l'eleve:\n");
            printf("\nNom: ");
            scanf("%s", nom);
            printf("\nPrenom: ");
            scanf("%s", prenom);
            file = ouvrir(fname1, 'A');
            int tai = strlen(prenom) + strlen(nom); //calcule de la taille du champs variable
            sprintf(taii, "%03d", tai);         //la convertir en un entier
            strcpy(info, "");
            strcat(info, taii);                 //concatener les differents champs
            strcat(info, "0");
            printf("\nGenre (F/M):\t");
            scanf("%s", char_genre);
            printf("\nIdentifiant de l'eleve:\t");
            scanf("%d", &intid_elev);
            sprintf(id_elev,"%04d",intid_elev);
            strcat(info, id_elev);
            printf("\nIdentifiant de la classe (eg. 54):\t");
            scanf("%s", id_class);
            strcat(info, id_class);
            strcpy(NomPrenom, "");
            strcat(NomPrenom, nom);
            strcat(NomPrenom, ".");
            strcat(NomPrenom, prenom);
            strcat(nom, ".");
            strcat(nom, prenom);
            strcat(nom, char_genre);
            strcat(info, nom);
            RempNotes(id_class[0], chaine);
            chaine[23] = '\0';
            strcat(info, chaine);
            InsertionTOVC(file, fname1, info, id_class, NomPrenom);
            printf("\n\n\t>> Eleve ajoute avec succes");
            fermer(file);
            ///Mise-à-jour de la table d'index
            RechTOVC(file, fname1, id_class, NomPrenom, & trouv, & bloc, & pos);   //recherche pour récuperer l'emplacement de l'enregistrement
            RechDichoIndexID(IndexID, NbE1, id_elev, & trouv, & k);             //recherche dichotomique de l'identifiant dans la table d'index
            if (trouv == 0) {
                NbE1++;
                int m = NbE1;
                while (m > k) {         //mettre-à-jour la table d'index (décaler)
                    IndexID[m] = IndexID[m - 1];
                    m--;
                }
                IndexID[k].eff = 0;             //mettre le champs effacé à 0
                IndexID[k].i = bloc;            //sauvegarder le numéro du bloc
                IndexID[k].j = pos;             //sauvegarder la position dans le bloc
                strcpy(IndexID[k].id_elev, id_elev);    //sauvegarder l'identifiant de l'eleve
            }
            system("Pause");
            goto menuprincipal;
        }

        case 4:
            system("cls");
        printf("\n\n\n________________________________________________________________________________________________________________\n");
        printf("\n                                              SUPPRESSION D'UN ELEVE                                    ");
        printf("\n_________________________________________________________________________________________________________________\n\n\n");
        printf("\nEntrez les informations concernant l'eleve:\n");
        printf("Nom: ");
        scanf("%s", nom);
        printf("Prenom: ");
        scanf("%s", prenom);
        strcat(nom, ".");
        strcat(nom, prenom);
        printf("\nNom: %s", nom);
        printf("\nIdentifiant de la classe: ");
        scanf("%s", id_class);
        SuppLogiqueTOVC(file, fname1, nom, id_class);       //suppression logique de l'enregistrement
        ///Mise-à-jour de la table d'index
        int trouv, bloc, pos, k;
        RechTOVC(file, fname1, id_class, NomPrenom, & trouv, & bloc, & pos);
        RechDichoIndexID(IndexID, NbE1, id_elev, & trouv, & k);
        if (trouv == 1) {
            IndexID[k].eff = 1;             //positionner le champs éffacé à 1
        }
        system("pause");
        goto menuprincipal;
        case 5:
            system("cls"); {
            printf("\n\n\n________________________________________________________________________________________________________________\n");
            printf("\n                                           MISE-A-JOUR DES INFORMATIONS                                   ");
            printf("\n_________________________________________________________________________________________________________________\n\n\n");
            printf("\n\n [1] : Mise-a-jour de la salle \n\n");
            printf("\n [2] : Mise-a-jour des notes \n\n");
            printf("\n\n Entrez votre choix:\t");
            int choix1;
            scanf("%d", & choix1);

            switch (choix1) {
                system("pause");
            case 1:
                system("cls"); {
                    printf("\n\n\n________________________________________________________________________________________________________________\n");
                    printf("\n                                             MISE-A-JOUR DE LA SALLE                                         ");
                    printf("\n_________________________________________________________________________________________________________________\n\n\n");
                    file = ouvrir(fname1, 'A');
                    printf("\n        >> Entrez les informations concernant l'eleve:\n");
                    printf("Nom:\t");
                    scanf("%s", nom);
                    printf("Prenom:\t");
                    scanf("%s", prenom);
                    strcat(nom, ".");
                    strcat(nom, prenom);
                    printf("\nNiveau de scolarisation (Entrez 'P' pour préparatif):\t");
                    scanf("%s", id_class);
                    printf("\nSalle ou groupe affecté:\t");
                    scanf(" %c", & char_salle);
                    MAJ_Salle(file, fname1, nom, id_class, char_salle);
                    system("pause");
                    goto menuprincipal;
                }
            case 2:
                system("cls"); {
                    printf("\n\n\n________________________________________________________________________________________________________________\n");
                    printf("\n                                            MISE-A-JOUR DES NOTES                                         ");
                    printf("\n_________________________________________________________________________________________________________________\n\n\n");

                    file = ouvrir(fname1, 'A');
                    printf("\n        >> Entrez les informations concernant l'eleve:\n");
                    printf("Nom:\t");
                    scanf("%s", nom);
                    printf("Prenom:\t");
                    scanf("%s", prenom);
                    strcat(nom, ".");
                    strcat(nom, prenom);
                    printf("\nIdentifiant de classe:\t");
                    scanf("%s", id_class);
                    printf("\n\nProgramme des matieres\n\n");
                    printf("\n1. Anglais");
                    if (id_class[0] == 'P')
                        printf("\t/");
                    printf("\n2. Arabe");
                    printf("\n3. Dessin");
                    if (id_class[0] == 'P')
                        printf("\t/");
                    printf("\n4. ECivique");
                    printf("\n5. EIslamique");
                    printf("\n6. Francais");
                    if (id_class[0] == 'P')
                        printf("\t/");
                    printf("\n7. HistGeo");
                    if (id_class[0] == 'P' | id_class[0] == '1')
                        printf("\t/");
                    printf("\n8. Maths");
                    printf("\n9. Musique");
                    if (id_class[0] == 'P')
                        printf("\t/");
                    printf("\n10. Tamazight");
                    printf("\n11. Technologie");
                    printf("\n\n      >> Entrez le numero correspondant a la matiere a mettre a jour, ainsi que la note/20:\n");
                    scanf("%d", & matiere);
                    printf("\t");
                    scanf("%s", note);
                    MAJ_note(file, fname1, nom, id_class, matiere, note);
                    system("pause");
                    goto menuprincipal;
                }
            }
        }
        case 6:
            system("cls"); {
            printf("\n\n\n________________________________________________________________________________________________________________\n");
            printf("\n                                           ARCHIVAGE DES RESULTATS                                   ");
            printf("\n_________________________________________________________________________________________________________________\n\n\n");
            printf("\n\n\t\t>> Entrez le nom du fichier archive: ");
            scanf("%s", fname2);
            CreationArchiv(file, fname1, arch, fname2);
            AffichTnOF(arch, fname2);
            system("pause");
            goto menuprincipal;
        }
         case 7:
            system("cls"); {
            printf("\n\n\n________________________________________________________________________________________________________________\n");
            printf("\n                                             REQUETE A INTERVALLE                                   ");
            printf("\n_________________________________________________________________________________________________________________\n\n\n");
            printf("\n\n\t>> Entrez la moyenne minimale:\t");
            float inf,sup;
            scanf("%f",&inf);
            printf("\n\n\t>> Entrez la moyenne maximale:\t");
            scanf("%f",&sup);
            printf("\n\n\t>> Entrez la salle sur laquelle porte cette requete (eg. 54):\t");
            char salle[3]="";
            scanf("%s",salle);
            Requete(inf,sup,salle,0);
            system("pause");
            goto menuprincipal;
        }

        end: return 0;
    }
    }
}

///---------------------------------------------------------------------------------------------------------------///
//Procédure qui demande à l'utilisateur de remplir un tableau de notes selon le niveau
void RempNotes(char niveau, char chaine[max_info])
{
    char mark[2];
    strcpy(chaine, "");
    printf("\n\n\t>>Veuillez introduire les notes correspondantes sur 20\n");
    if (niveau == 'P') {            //si c'est un niveau preparatoire
        strcpy(chaine, "XX");
        printf("\nArabe: ");
        scanf("%s", mark);
        strcat(chaine, mark);
        strcat(chaine, "XX");
        printf("E/Civique: ");
        scanf("%s", mark);
        strcat(chaine, mark);
        printf("E/Islamique: ");
        scanf("%s", mark);
        strcat(chaine, mark);
        strcat(chaine, "XX");
        strcat(chaine, "XX");
        printf("Maths: ");
        scanf("%s", mark);
        strcat(chaine, mark);
        strcat(chaine, "XX");
        strcat(chaine, "XX");
        printf("Technologie: ");
        scanf("%s", mark);
        strcat(chaine, mark);
        printf("\nNotes: %s", chaine);
    } else {
        if (niveau == '1' | niveau == '2') {        //si c'est une 1e/2e année
            printf("\nAnglais: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Arabe: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Dessin: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("E/Civique: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("E/Islamique: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Francais: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            strcat(chaine, "XX");
            printf("Maths: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Musique: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Tamazight: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Technologie: ");
            scanf("%s", mark);
            strcat(chaine, mark);
        } else {                        //3e/4e/5e année
            printf("\nAnglais: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Arabe: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Dessin: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("E/Civique: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("E/Islamique: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Francais: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Hist/Geo: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Maths: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Musique: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Tamazight: ");
            scanf("%s", mark);
            strcat(chaine, mark);
            printf("Technologie: ");
            scanf("%s", mark);
            strcat(chaine, mark);
        }
    }
    chaine[23] = '\0';
}

/// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ARCHIVAGE - - - - - - -  - - - - - - - - - - - - - - - - - - - - - - - - - -//
//procedure de creation du fichier archive, Index sur les moyennes ainsi que les salles en 0 parcours du fichier au lieu de 2
void CreationArchiv(TOVC * file, char fname[], TnOF * arch, char fname2[]) {
    char ancien_id[3];
    strcpy(ancien_id, "P1");
    int i1 = 1, j1 = 0, int_taille, i2 = 1, j2 = 0, sauv_i, sauv_j;
    char tmp_taille[4] = "", tmp_eff[2] = "", tmp_elev[5] = "", save_elev[5] = "", tmp_id[3] = "", save_id[3] = "", tmp_gn[8] = "", tmp_notes[23] = "", tmp_RechID[3] = "";

    char * tmp_nom = malloc(sizeof(char) * 30);
    float moy;
    DossierScolaire e;
    file = ouvrir(fname, 'A');
    ///                                                  **Creation du fichier d'archive**
    arch = ouvrir_TnOF(fname2, 'N');
    fermer_TnOF(arch);
    arch = ouvrir_TnOF(fname2, 'A');
    liredir_TnOF(arch, i2, & buff1);
    DossierScolaire tab_tri[30];                         //tableau pour trier les Dossiers en fonction de la moyenne
    int m = 0;                                           //pour parcourir les élements du tableau
    int y = 0;                                           //pour parcourir l'index des moyennes
    while ((i1 < entete(file, 1)) || ((i1 == entete(file, 1)) && (j1 < entete(file, 3)))) {     //parcours séquentiel du fichier

        recup_chaine(file, & i1, & j1, 3, tmp_taille);          //récuperer les champs de l'enregistrement
        int_taille = atoi(tmp_taille);
        recup_chaine(file, & i1, & j1, 1, tmp_eff);
        recup_chaine(file, & i1, & j1, 4, tmp_elev);
        recup_chaine(file, & i1, & j1, 2, tmp_id);
        recup_chaine(file, & i1, & j1, int_taille + 1, tmp_nom);
        recup_chaine(file, & i1, & j1, 1, tmp_gn);
        recup_chaine(file, & i1, & j1, 22, tmp_notes);
        strcpy(e.id_eleve, tmp_elev);
        strcpy(e.Tab[0].ID, tmp_id);
        moy = moyenne(tmp_notes);
        e.Tab[0].Moyenne = moy;                                 //sauvegarder la moyenne et l'année courante dans la premiere case
        for (int p = 1; p <= 6; p++) {                          //initialiser les autres à 00.00 pour dire (vides)
            e.Tab[p].Moyenne = 00.00;
            strcpy(e.Tab[p].ID, "/");                           //initialiser les salles à / pour dire (vides)
        }

        if (strcmp(tmp_id, ancien_id) == 0) {                   //sauvegarder l'enregistrement dans un tableau
            tab_tri[m] = e;
            m++;
        }
        if ((strcmp(tmp_id, ancien_id) != 0) || (strcmp(tmp_id, "") == 0)) {    //si tous les eleves de la salle ont été récupéré
            TriRapideMoyenne(tab_tri, 0, m - 1, 0);                             //on trie leurs dossiers selon la moyenne

            ///Remplissage de la table d'index sur les moyennes (non-dense)
            IndexM[y].Min = tab_tri[m - 1].Tab[0].Moyenne;                      //sauvegarder la moyenne minimale
            IndexM[y].Max = tab_tri[0].Tab[0].Moyenne;                          //et maximale
            IndexM[y].bloc = i2;                                                //ainsi que l'adresse du début de la salle
            IndexM[y].pos = j2;
            strcpy(IndexM[y].salle, ancien_id);                                 //et l'identifiant de l'élève
            y++;

            for (int ind = 0; ind < m; ind++) {                                 //tab_tri étant maintenant ordonné, on insère les enregistrements

                if (j2 < TnOF_size)
                {
                    buff1.tab[j2] = tab_tri[ind];
                    j2++;
                } else                                                         //on sauvegarde l'enregsitrement si le buffer n'est pas encore plein
                {
                    buff1.Nb = j2 - 1;
                    ecriredir_TnOF(arch, i2, buff1);                            //sinon on l'écrit
                    buff1.tab[0] = tab_tri[ind];
                    i2++;                                                   //numéro du prochain bloc à lire
                    liredir_TnOF(arch, i2, & buff1);                        //lecture du prochain bloc
                    j2 = 1;                                                 //se positionner au début
                }
            }
            tab_tri[0] = e;
            m = 1;
            strcpy(ancien_id, tmp_id);
        }
    }
                                                                                //Traitement des dossiers de la dernière salle
    TriRapideMoyenne(tab_tri,0, m, 0);
    IndexM[y].Max = tab_tri[0].Tab[0].Moyenne;
    IndexM[y].Min = tab_tri[m - 1].Tab[0].Moyenne;
    IndexM[y].bloc = i2;
    IndexM[y].pos = j2;
    strcpy(IndexM[y].salle, ancien_id);
    for (int ind = 0; ind < m; ind++) {                                 //meme opérations.
        if (j2 < TnOF_size) {
            buff1.tab[j2] = tab_tri[ind];
            j2++;
        } else {
            buff1.Nb = j2 - 1;
            ecriredir_TnOF(arch, i2, buff1);
            i2++;
            i2 = alloc_bloc_TnOF(arch);
            liredir_TnOF(arch, i2, & buff1);
            j2 = 1;
        }
    }
    buff1.Nb = j2 - 1;
    ecriredir_TnOF(arch, i2, buff1);                                                //Ecriture du dernier bloc
    aff_entete_TnOF(arch, 1, i2 );                                                   //mise-à-jour de l'entete
    aff_entete_TnOF(arch, 2, j2  + (i2 - 1) * TnOF_size);
    NbE2 = y +1;            //taille de l'index des identifiants
    fermer(file);           //fermeture des fichiers
    fermer_TnOF(arch);
}

//procedure d'affichage du contenu du fichier d'archives
void AffichTnOF(TnOF * file, char fname[]) {
    file = ouvrir_TnOF(fname, 'A');
    int i = 1;
    int j;
    int count = 1;
    while (i <= entete_TnOF(file, 1)) {
        j = 0;
        liredir_TnOF(file, i, & buff1);
        while (j <= buff1.Nb)
        {
            printf("\n\n\n\t\t\t\tDossier n%d\n", count);
            printf("\n\t _______________________________________________________");
            printf("\n\t|                                                       |\n");
            printf("\t|ID Eleve: %s                                         |", buff1.tab[j].id_eleve);
            printf("\n\t|\t\t\t**Parcours**\t\t\t|");
            printf("\n\t|                                                       |");
            for (int u = 0; u < 6; u++) {
                printf("\n\t|\t\tAnnee: %s\t\tMoyenne: %.2f\t|", buff1.tab[j].Tab[u].ID, buff1.tab[j].Tab[u].Moyenne);
            }
            printf("\n\t|_______________________________________________________|\n");
            count++;
            j++;
        }
        i++;
    }
    fermer_TnOF(file);
}


///Tri des n-iemes moyennes dans les dossiers scolaires du fichier d'archive

//Procédure qui permute entre 2 Dossiers Scolaires
void PermutMoy(DossierScolaire * a, DossierScolaire * b) {
    DossierScolaire t = * a;
    * a = * b;
    * b = t;
}

//Fonction pour chercher la position de la partition
int partitionMoy(DossierScolaire array[], int low, int high, int n) {
    DossierScolaire pivot = array[high]; //définir l'element le plus à droite comme pivot
    int i = (low - 1); //Récuperer l'indice de l'element le plus à gauche
    //parcourir les elements du tableau et les comparer avec le pivot
    for (int j = low; j < high; j++) {
        if (array[j].Tab[n].Moyenne >= pivot.Tab[n].Moyenne) {
            i++;
            PermutMoy( & array[i], & array[j]); //Si un element est plus grand que le pivot, on le permut avec l'element plus a gauche
        }
    }
    PermutMoy( & array[i + 1], & array[high]);
    return (i + 1);
}

void TriRapideMoyenne(DossierScolaire array[], int low, int high, int n) {
    if (low < high) {
        int pi = partitionMoy(array, low, high, n);
        TriRapideMoyenne(array, low, pi - 1, n);                    ///appel récursif pour la partie gauche du pivot
        TriRapideMoyenne(array, pi + 1, high, n);                    ///appel récursif pour la partie droite du pivot
    }
}


///Tri IndexID sur l'identifiant de l'élève

void PermutIndexID(Index_IDcouple * a, Index_IDcouple * b) {
    Index_IDcouple t = * a;
    * a = * b;
    * b = t;
}

//Fonction pour chercher la position de la partition
int partitionIndexID(Index_IDcouple array[], int low, int high) {
    Index_IDcouple pivot = array[high];                         //définir l'element le plus à droite comme pivot
    int i = (low - 1);                                          //Récuperer l'indice de l'element le plus à gauche

    for (int j = low; j < high; j++) {                          //parcourir les elements du tableau et les comparer avec le pivot
        if (strcmp(array[j].id_elev, pivot.id_elev) <= 0) {
            i++;
            PermutIndexID( & array[i], & array[j]);             //Si un element est plus grand que le pivot, on le permut avec l'element plus a gauche
        }
    }
    PermutIndexID( & array[i + 1], & array[high]);
    return (i + 1);
}

void TriRapideIndexID(Index_IDcouple array[], int low, int high) {
    if (low < high) {
        int pi = partitionIndexID(array, low, high);
        TriRapideIndexID(array, low, pi - 1);                     ///appel récursif pour la partie gauche du pivot
        TriRapideIndexID(array, pi + 1, high);                      ///appel récursif pour la partie droite du pivot
    }
}

//Procédure de la recherche dichotomique d'un eleve dans la table d'index des identifiants IndexID
void RechDichoIndexID(Index_IDcouple IndexID[IndexIDsize], int NbE, char id_elev[5], int * trouv, int * i) {
    int bi, bs;                                      //définir les bornes inférieure et supérieure
    bi = 1;
    bs = NbE;* trouv = 0;
    while (bi <= bs && * trouv == 0) {
        * i = (bi + bs) / 2;                                        //calcul de l'indice de la case du milieu
        if (strcmp(id_elev, IndexID[ * i].id_elev) < 0) {           //si l'id actuel est inférieur
            bs = * i - 1;                                           //recherche dans la partie en haut
        } else {
            if (strcmp(id_elev, IndexID[ * i].id_elev) > 0)         //si l'id actuel est supérieur
                bi = * i + 1;                                       //recherche dans la partie en bas
            else
                * trouv = 1;                                          //si c'est égal, on sort de la boucle
        }
    }
    if ( * trouv == 0)                                               //si l'eleve recherché n'existe pas, on retourne là ou on doit l'insérer
        *i = bi;
}

//Procédure de la recherche dichotomique d'une salle dans la table d'index des moyennes IndexM
void RechDichoIndexMoyennes(Index_Moyennecouple IndexMoyenne[IndexMoyennesize], int NbE, char salle[], int * trouv, int * i) {
    int bi, bs;
    bi = 0;
    bs = NbE - 1;* trouv = 0;
    if (strcmp(salle, "P1") == 0) {
        * trouv = 1;
        * i = 0;
    }
    if (strcmp(salle, "P2") == 0) {
        * trouv = 1;
        * i = 1;
    }
    if (strcmp(salle, "P4") == 0) {
        * trouv = 1;
        * i = 3;
    }
    if (strcmp(salle, "P3") == 0) {
        * trouv = 1;
        * i = 2;
    }
    while (bi <= bs && * trouv == 0) {
        * i = (bi + bs) / 2;

        if (atoi(salle) < atoi(IndexMoyenne[ * i].salle)) {
            bs = * i - 1;
        } else {
            if (atoi(salle) > atoi(IndexMoyenne[ * i].salle))
                bi = * i + 1;
            else
                *
                trouv = 1;
        }
    }
    if ( * trouv == 0)
        *
        i = bi;
}

//Procédure de la requete ;  affiche les informations des eleves de la salle 'salle' dont la moyenne 'z' est comprise entre inf et sup
void Requete(float inf, float sup, char salle[], int z) ///Requete sur la kieme moyenne
{
    int n, i, j,trouv, p;
    RechDichoIndexMoyennes(IndexM, NbE2, salle, & trouv, & i);              ///Recherche dichotomioque de la salle en question dans l'index
    float Min = IndexM[i].Min;                                          ///Récupérer la moyenne minimale de la salle
    float Max = IndexM[i].Max;                                          ///Récuperer la moyenne maximale de la salle
    printf("\nMoyenne Minimale: %.2f\tMoyenne Maximale: %.2f", Min, Max);
    if (sup < Min)                                                  ///Cas ou l'on cherche des moyennes plus petites que la moyenne minimale de la salle
        printf("\n\t!!!Requete Impossible!!!");
    else {
        int k = IndexM[i].bloc;                                     ///Récuperer l'adresse de la salle dans l'archive
        int l = IndexM[i].pos;
        int stop = 0;
        arch = ouvrir_TnOF(fname2, 'A');                            ///Ouverture du fichier archive
        printf("\n %d %d",k,l);
        system("pause");
        liredir_TnOF(arch, k, & buff1);                             ///lecture du bloc contenant la salle

        while (stop == 0) {
            if (buff1.tab[l].Tab[0].Moyenne <= sup && buff1.tab[l].Tab[0].Moyenne >= inf)   ///Si la moyenne vérifie la requete
            {
                RechDichoIndexID(IndexID, NbE1, buff1.tab[l].id_eleve, & trouv, & p);   ///Recherche dichotomique de l'identifiant dans l'index dense
                if (IndexID[p].eff == 0)                                                    ///      des identifiants
                {
                    printf("\nMoyenne: %.2f", buff1.tab[l].Tab[0].Moyenne);
                    AffichEnreg(file, fname1, IndexID[p].i, IndexID[p].j);           ///Récupérer l'enregistrement du fichier de données
                }
                l++;                                                                ///passer au prochain enregistrement
            } else {
                if (l < buff1.Nb)                                                   ///S'il reste des enregistrements à traiter dans le bloc
                {
                    l++;
                } else                                                          ///On a traité tous les enregistrements du bloc, passer au prochain
                {
                    k++;
                    liredir_TnOF(arch, k, & buff1);
                    l = 0;
                }
            }
            if ((buff1.tab[l].Tab[0].Moyenne < inf) | strcmp(buff1.tab[l].Tab[0].ID, salle) != 0)       ///Si on trouve un enregistrement dont la moyenne
                stop = 1;        ///arreter la requete                                                    ///est plus petite que la borne inférieure de l'intervalle recherché, ou si on entre dans une autre salle
        }
    }
}

//Procédure du chargement initial du fichier de données de la simulation (chargement de la 1ere promotion)
void ChgmtSimul(TOVC * file, char fname[], int nb_elevs, int nb_salle) {
    ///Chargement d'une seule promotion en prepa
    ///Nombre de salles de la promo:
    int i = 0, l = 0, somme = 0, bi3 = 1, taille, salle_elevs = (nb_elevs / 6) / nb_salle;
    ///variables pour generer l'information
    char tmp_nom[50], chaine[30] = "", tai[4], id_class[3], niveau_char[2], salle_char[2], NomPrenom[max_info], info[max_info], temp[50];

    ///tableaux des donnees generes aleatoirement
    char * tab_id_elev[nb_elevs];
    char * tab_nom[nb_elevs];
    char * tab_nom_tri[nb_elevs];
    char * tab_id[nb_elevs];
    AleaDonnees(nb_elevs, tab_id_elev, tab_nom);
    file = ouvrir(fname, 'N'); ///Creation d'un fichier vide
    int k = alloc_bloc(file);
    fermer(file);
    /// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ///
    file = ouvrir(fname, 'A');
    i = 0;
    strcpy(id_class, "P");
    for (int salle = 1; salle <= nb_salle; salle++) {
        sprintf(salle_char, "%d", salle);                          ///pour toutes les salles définies
        id_class[1] = salle_char[0];
        id_class[2] = '\0';
        int sauv_i = i;
        int tmp_i = i;
        for (i = sauv_i; i < nb_elevs / nb_salle + sauv_i; i++) {
            for (int j = i + 1; j < nb_elevs / nb_salle + sauv_i; j++)      ///trier les eleves de la salle selon l'ordre alphabétique
            {
                if (strcmp(tab_nom[i], tab_nom[j]) > 0) {
                    strcpy(temp, tab_nom[i]);
                    strcpy(tab_nom[i], tab_nom[j]);
                    strcpy(tab_nom[j], temp);
                }
            }
        }
        for (int m = 0; m < nb_elevs / nb_salle; m++) {
            tab_nom_tri[m] = malloc(sizeof(char) * 50);                      ///Sauvegarder les noms tries dans un tableau
            sprintf(tab_nom_tri[m], tab_nom[sauv_i]);
            sauv_i++;
        }
        for (int m = 0; m < nb_elevs / nb_salle; m++) {                     ///pour tous les étudiants de la salle en question
            strcpy(info, "");
            taille = strlen(tab_nom_tri[m]) - 2;
            sprintf(tai, "%03d", taille);
            strcat(info, tai);
            strcat(info, "0");
            strcat(info, tab_id_elev[tmp_i]);
            strcat(info, id_class);
            strcat(info, tab_nom_tri[m]);
            strcpy(chaine, "");
            gen_notes(id_class, chaine, bi3);                               ///générer les autres champs de l'enregistrement aleatoirement
            if (bi3 > 13)
                bi3 = 10;
            strcat(info, chaine);
            ecrire_chaine(file, & k, & l, strlen(info), info);
            tmp_i++;
            somme += strlen(info);         //somme contiendra le total des caractères insérés dans le fichier
        }
    }
    aff_entete(file, 1, k);                                                 ///mise-à-jour de l'entete
    aff_entete(file, 2, somme);
    aff_entete(file, 3, l);
    fermer(file);
    file = ouvrir(fname, 'A');
    AffichTOVC(fname);                                                      ///Affichage du fichier
    fermer(file);               //fermeture du fichier
}

//Fonction qui retourne un réel aleatoir compris entre 'left' et 'right'
float GenMoy(float left, float right) {
    time(NULL);
    return (((float) rand() / (float)(RAND_MAX)) * right);
}


//Procédure de simulation de 5 ans d'étude
void Simulation() {
    ChgmtSimul(file, fname1, nb_elevs, nb_salle);
    CreatIndexID(IndexID, file, fname1, & NbE1);
    CreationArchiv(file, fname1, arch, fname2);
    char id_salle[3];
    int i = 1, j = 0;
    float moy;
    float bi = 5.00, bs = 20.00;                          //définir les bornes inférieure et supérieure pour generer les moyennes aleatoirement
    arch = ouvrir_TnOF(fname2, 'A');
    liredir_TnOF(arch, i, & buff1);
    while (i < entete_TnOF(arch, 1) || (i == entete_TnOF(arch, 1) && j <= entete_TnOF(arch, 2))) {  //parcourt séquentiel du fichier initial contenant la 1ere promo
        if (id_salle, buff1.tab[j].Tab[0].Moyenne >= 10) {                                          //si la moyenne en préparatoire >= 10, l'eleve passe à l'année prochaine
            int stop = 0;
            int niveau = 1;
            strcpy(id_salle, buff1.tab[j].Tab[0].ID);
            while ((stop == 0) && (niveau <= 5)) {                                   //pour toutes les années qui suivent (1,2,3,4 et 5)
                moy = GenMoy(bi, bs);                                                //générer aleatoirement des moyennes et s'arreter dès que la moyenne < 10 (cas de redouble)
                id_salle[0] = niveau + '0';
                strcpy(buff1.tab[j].Tab[niveau].ID, id_salle);                      //sauvegarder l'identifiant de la classe
                buff1.tab[j].Tab[niveau].Moyenne = moy;                             //sauvegarder le dossier de l'élève dans le buffer
                if (moy < 10)
                    stop = 1;                                                       //s'arreter si l'eleve redouble
                else
                    niveau++;                                                       //passer au niveau suivant si l'eleve est admis
            }

        }
        if (j < buff1.Nb)
            j++;
        else {                                                                      //si le buffer est plein, on l'écrit dans le fichier d'archives.
            ecriredir_TnOF(arch, i, buff1);
            j = 0;
            i++;
            liredir_TnOF(arch, i, & buff1);
        }
    }
    ecriredir_TnOF(arch, i, buff1);                                                 //ecriture du dernier bloc
    fermer_TnOF(arch);
    printf("\n\n\t>>Fin de la simulation.\nCliquez sur Entrer pour consulter les dossiers scolaires de la promotion: ");
    system("pause");
    AffichTnOF(arch, fname2);                                                       //affichage du contenu de l'archive des 5 ans
}

//Procédure pour lister tous les étudiants qui ont eu +12 sur les 5 ans
void Lister() {
    arch = ouvrir_TnOF(fname2, 'A');
    int i = 1;
    int j, p, trouv;
    while (i <= entete_TnOF(arch, 1)) {
        j = 0;
        int lister;                                                           ///booléen qui indique qu'un enregistrement vérifie la requete > 12
        liredir_TnOF(arch, i, & buff1);
        while (j <= buff1.Nb) {
            lister = 0;
            for (int u = 0; u < 6; u++) {
                if (buff1.tab[j].Tab[u].Moyenne < 12)                       ///Si on trouve au moins une moyenne qui est < à 12 on remet lister à faux
                {
                    lister = 1;
                }
            }
            if (lister == 0) {
                RechDichoIndexID(IndexID, NbE1, buff1.tab[j].id_eleve, & trouv, & p);    ///Recherche dichotomique de l'identifiant dans l'index dense
                if (IndexID[p].eff == 0)                                                         ///      des identifiants
                {
                    AffichEnreg(file, fname1, IndexID[p].i, IndexID[p].j);               ///Récupérer l'enregistrement du fichier de données
                }
            }
            j++;
        }
        i++;
    }
    fermer_TnOF(arch);
}

//Procédure qui effectue le transfert d'un eleve (Insertion de ses informations en fichier de données + Insertion de son archive en fichier des archives)
void Transfert()
{
            float moy;
            DossierScolaire e;
            int stop = 0  ;
            char chaine[23],info[max_info]="",nom[40]="",prenom[40]="",taii[4]= "",char_genre[2]="",id_elev[5]="",id_class[3]="",NomPrenom[50]="";
            printf("\n\n                                           Informations  de l'eleve                             ");
            printf("\n                                           ------------------------                             \n\n");
            printf("\nNom: ");                          //Récuperer les informations de l'eleve depuis l'utilisateur
            scanf("%s", nom);
            printf("\nPrenom: ");
            scanf("%s", prenom);
            int tai = strlen(prenom) + strlen(nom);         //calcul de la taille du champs variable
            sprintf(taii, "%03d", tai);                     //convertir en un entier
            strcpy(info, "");
            strcat(info, taii);                              //concaténation des champs
            strcat(info, "0");           //caractère d'effacement
            printf("\nGenre (F/M) : ");
            scanf("%s", char_genre);
            printf("\nIdentifiant (sur 4 positions): ");
            scanf("%s", id_elev);
            strcat(info, id_elev);
            printf("\nIdentifiant de la  classe (EX. P1): ");
            scanf("%s", id_class);
            id_class[2] = '\0';
            char niveau = id_class[0];
            int niveau_int = niveau - '0';
            strcat(info, id_class);
            strcpy(NomPrenom, "");
            strcat(NomPrenom, nom);
            strcat(NomPrenom, ".");
            strcat(NomPrenom, prenom);
            strcat(nom, ".");
            strcat(nom, prenom);
            strcat(nom, char_genre);
            strcat(info, nom);
            RempNotes(id_class[0], chaine);                               //Remplir le tableau de notes de l'année actuelle de l'élève
            chaine[23] = '\0';
            strcat(info, chaine);                                         //Récuperer toutes les moyennes précedentes de l'eleve depuis l'utilisateur
            printf("\n\n                                            Dossier  de l'eleve                             ");
            printf("\n                                             -------------------                             \n\n");
            for (int count = 0 ; count < niveau_int ; count++)
            {
                printf("\nVeuillez introduire la moyenne correspondante: [%d] \t",count);
                scanf("%f",&moy);
                if (moy > 10)
                {
                e.Tab[count].Moyenne = moy;
                strcpy(e.Tab[count].ID,"//");                           ///pour dire, identifiant de classe n'appartenant pas a cette ecole

                }
                else                                                    ///si une des moyennes <10, il y a incohérence (l'eleve n'aurait pas pu passer en un niveau plus haut)
                {
                    printf("\n\nTransfert refuse");                     ///donc le transfert est refusé
                    stop = 1;
                }
            }
           if (stop == 0)                                               ///si toutes les moyennes de l'eleve jusqu'à présent sont >= 10
           {
            file = ouvrir(fname1,'A');                                  //ouverture des fichiers de données et d'archive
            arch = ouvrir_TnOF(fname2,'A');
            strcpy(e.id_eleve,id_elev);
            InsertionTnOF(arch,e);                                      ///Insérer le dossier de l'eleve dans le fichier d'archive
            InsertionTOVC(file, fname1, info, id_class, NomPrenom);     ///Inserer l'eleve dans la salle voulue
            printf("\n\n\t\t>> Eleve transfere avec succes");
            fermer(file);
            int trouv,bloc,pos,k;                                        ///Mise-à-jour de la table d'index
            RechTOVC(file, fname1, id_class, NomPrenom, & trouv, & bloc, & pos);  //recherche dans le fichier de données pour récuperer l'emplacement de l'enregistrement
            RechDichoIndexID(IndexID, NbE1, id_elev, & trouv, & k); //recherche dichotomique de l'identifiant de l'eleve dans la table d'index
            NbE1++;
                int m = NbE1;       //boucle de décalages jusqu'à la fin de la table d'index
                while (m > k) {
                    IndexID[m] = IndexID[m - 1];
                    m--;
                }
                IndexID[k].eff = 0;     //positionner le champs d'effacement à 0
                IndexID[k].i = bloc;    //sauvegarder l'emplacement dans le fichier de données
                IndexID[k].j = pos;
                strcpy(IndexID[k].id_elev, id_elev);    //sauvegarder l'identifiant

        }
}

