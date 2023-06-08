#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#define TOVC_size 10000
#define TnOF_size 100
#define max_info 1000
#define IndexIDsize 600
#define IndexMoyennesize 1000

///Déclaration des structures TOVC

///Structure d'un bloc
typedef struct Tbloc {
  char chaine[TOVC_size];
}
Tbloc;

typedef struct Tbloc Buffer;
Buffer buff;

typedef struct Entete {
  int adr_dernier_bloc;          ///Contient l'adresse du dernier bloc
  int nbenreg;                   ///nombre des enregistrements insérés
  int indice_libre;              ///premier indice libre dans le dernier bloc (tous les autres blocs sont pleins du au chevauchement)
  int nbsup;                     ///nombre des enregistrements supprimés logiquement
}
Entete;

typedef struct TOVC {
  FILE * F;
  Entete entete;
}
TOVC;

///Déclaration de la structure de TnOF

///Structure de l'enregistrement contenant le résultat d'une année
typedef struct Resultat {
  char ID[3];
  float Moyenne;
}
Resultat;

///Structure d'un dossier scolaire
typedef struct DossierScolaire {
  char id_eleve[5];     //Le dossier scolaire d'un eleve dont l'id est 'id_eleve' est un tableau contenant le résultat de ses années de scolarisation précédentes
  Resultat Tab[7];      //ainsi que l'identifiant de la classe;
}
DossierScolaire;


///Structures du fichier d'archive
typedef struct Tbloc_TnOF {
  int Nb;
  DossierScolaire tab[TnOF_size];
}
Tbloc_TnOF;

typedef struct Tbloc_TnOF Buffer_TnOF;
Buffer_TnOF buff1;

typedef struct Entete_TnOF {
  int Nbloc;                        ///adresse du dernier bloc
  int Nenreg;                       ///numéro du dernier enregistrement
}
Entete_TnOF;

typedef struct TnOF {
  FILE * file;
  Entete_TnOF Entete_TnOF;
}
TnOF;

///Déclaration de la table d'index sur l'id (dense)
//contient l'adresse de l'enregistrement dans le fichier de données, ainsi que l'état de l'enregistrement (éffacé ou pas)
typedef struct Index_IDcouple {
  int eff;
  int i;
  int j;
  char id_elev[5];
}
Index_IDcouple;

///Déclaration de la table d'index sur les moyennes (non-dense)
//Contient le min, max des moyennes de chaque salle ainsi que son adresse dans le fichier d'archive
typedef struct Index_Moyennecouple {
  char salle[3];
  float Min;
  float Max;
  int bloc;
  int pos;
}
Index_Moyennecouple;

///--------------------------------MACHINES ABSTRAITES----------------------------------//


//*****TOVC*****/

TOVC * ouvrir(char * filename, char mod);
void fermer(TOVC * pF);
int entete(TOVC * pF, int i);
void aff_entete(TOVC * pF, int i, int val);
void liredir(TOVC * pF, int i, Buffer * buf);
void ecriredir(TOVC * pF, int i, Buffer buf);
int alloc_bloc(TOVC * pF);

//*****TnOF*****//

TnOF * ouvrir_TnOF(char fname[], char mode);
void fermer_TnOF(TnOF * f);
void liredir_TnOF(TnOF * pF, int i, Buffer_TnOF * buff1);
void ecriredir_TnOF(TnOF * pF, int i, Buffer_TnOF buff1);
void aff_entete_TnOF(TnOF * f, int i, int val);
int entete_TnOF(TnOF * f, int i);
int alloc_bloc_TnOF(TnOF * pF);

///-----------------------------------AFFICHAGE--------------------------------------//
void bienvenu();
void menu();
void MenuPrincipal();
void AffichTOVC(char fname[]);
void AffichEnreg(TOVC * file, char fname[], int i, int j);
void AffichNotes(char chaine_notes[]);
void AffichTnOF(TnOF * file, char fname[]);
void AffichIndexM(Index_Moyennecouple IndexM[IndexMoyennesize], int NbE);
void AffichIndexID(Index_IDcouple IndexID[IndexIDsize], int NbE);

///---------------------------MANIPULATION DU FICHIER DE DONNEES-------------------------------//
void ChgmtInit(TOVC * file, char fname[], int total_elev, int nb_salle);
void RechTOVC(TOVC * file, char fname[], char RechID[], char RechNom[], int * trouv, int * i, int * j);
void InsertionTOVC(TOVC * file, char fname[], char e[max_info], char cle_id[], char cle_nom[]);
void SuppLogiqueTOVC(TOVC * file, char fname[], char nom[], char id_class[]);

///module qui récupère une chaine de 'nbcar' caractères à partir de la position j du bloc i du fichier dans buff_chaine
void recup_chaine(TOVC * file, int * i, int * j, int nbcar, char buff_chaine[]);
///module qui écrit une chaine 'buff_chaine' à partir de la position j dans le bloc i du  file
void ecrire_chaine(TOVC * file, int * i, int * j, int nbcar, char buff_chaine[]);

///---------------------------Génération de l'enregistrement------------------------------//
///module qui genère aléatoirement un id unique d'un élève sur 4 caractères
void gen_id_eleve(int tab_id[], int * id, int BI, int BS);

///module qui genère aléatoirement l'id de la classe sur 2 caractères (1 pour le niveau, 2 pour la salle séquentielle par année)
void gen_id_class(char * id_class);

///module qui selectionne aleatoirement un nom et un prenom a partir des fichiers textes
void gen_nom_gprenom(char nom[], int BI, int BS);

///module de generation aleatoire d'un tableau de notes d'un eleve appartenant a un niveau donné, à partir d'un fichier texte
void gen_notes(char id_class[], char chaine_notes[], int BI);

///module qui génère aleatoirement un enregistrement
void gen_info(char id_class[], char info[max_info], char NomPrenom[], int * tab_id[], int * bi1, int * bs1, int * bi2, int * bs2, int * bi3);

///--------------------------------Modules de Manipulation des données-----------------------------------//
///Fonction qui retourne 0 si la chaine 'chaine' ne contient pas des caractères numériques
int NoDigits(char chaine[]);

///Fonction qui divise la chaine 'NomPrenom' en deux sous chaines 'Nom' et 'Prenom' en fonction du délimiteur '.'
void Split_Nom_Prenom(char NomPrenom[], char Nom[], char Prenom[]);

///procedure de mise-à-jour de la salle correspondante à un eleve donné
void MAJ_Salle(TOVC * file, char fname[], char NomPrenom[], char Ancien_id[], char salle);

///procédure de mise-à-jour de la note de la matiere dont le numéro est 'matiere'
void MAJ_note(TOVC * file, char fname[], char NomPrenom[], char id_class[], int matiere, char note[]);

///module de remplissage d'un tableau de notes 'chaine' en fonction du niveau de scolarisation 'niveau'
void RempNotes(char niveau, char chaine[max_info]);

///---------------------------MANIPULATION DU FICHIER D'ARCHIVE ET INDEX-------------------------------//
void RechTnOF(TnOF * arch, char id_elev[], int * trouv, int * i, int * j);

void CreatIndexID(Index_IDcouple IndexID[IndexIDsize], TOVC * file, char fname[], int * NbE);

///module de creation d'un fichier d'archive à partir du fichier de données
void CreationArchiv(TOVC * file, char fname[], TnOF * arch, char fname2[]) ;

///fonction qui calcule la moyenne d'un élève à partir de son tableau de notes 'chaine'
float moyenne(char chaine[]);

///Procedure du tri rapide des dossiers scolaires par moyenne
void PermutMoy(DossierScolaire * a, DossierScolaire * b);
int partitionMoy(DossierScolaire array[], int low, int high, int n);
void TriRapideMoyenne(DossierScolaire array[], int low, int high, int n);

void PermutTabSalles(int * a, int * b);
int partitionTabSalles(int array[], int low, int high);
void TriRapideTabSalles(int array[], int low, int high);


void RechDichoIndexMoyennes(Index_Moyennecouple IndexMoyenne[IndexMoyennesize], int NbE, char salle[], int * trouv, int * i);
void RechDichoIndexID(Index_IDcouple IndexID[IndexIDsize], int NbE, char id_elev[5], int * trouv, int * i);
void Requete(float inf, float sup, char salle[], int z);
void Archivage_Res_Annee(int annee, TOVC * file, TnOF * arch);
float GenMoy(float left, float right);
void ChgmtSimul(file, fname1, total_elevs, nb_salle);
void Transfert();
void Simulation();
void Lister();
