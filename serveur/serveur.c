#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>

/**
 * @brief Structure Client pour regrouper toutes les informations du client.
 *
 * @param estOccupe 1 si le Client est connecté au serveur ; 0 sinon
 * @param dSC Socket de transmission des messages classiques au Client
 * @param pseudo Appellation que le Client rentre à sa première connexion
 * @param dSCFC Socket de transfert des fichiers
 * @param nomFichier Nomination du fichier choisi par le client pour le transfert
 */
typedef struct Client Client;
struct Client
{
	int estOccupe;
	long dSC;
	int idSalon;
	char *pseudo;
	long dSCFC;
	char nomFichier[100];
};

/**
 *  @brief Définition d'une structure Salon pour regrouper toutes les informations d'un salon.
 *
 * @param idSalon Identifiant du salon
 * @param estOccupe 1 si le salon existe ; 0 sinon
 * @param nom Appellation du salon, donné à la création (max 20)
 * @param description Description du salon, donné à la création (max 200)
 * @param nbPlace Nombre de place que peut accepter le salon, donné à la création
 */

typedef struct Salon Salon;
struct Salon
{
	int idSalon;
	int estOccupe;
	char *nom;
	char *description;
	int nbPlace;
};

/**
 * - MAX_CLIENT = nombre maximum de clients acceptés sur le serveur
 * - MAX_SALON = nombre maximum de salons sur le serveur
 * - TAILLE_PSEUDO = taille maximum du pseudo
 * - TAILLE_MESSAGE = taille maximum d'un message
 */
#define MAX_CLIENT 7
#define MAX_SALON 3
#define TAILLE_PSEUDO 20
#define TAILLE_MESSAGE 500

/**
 * - tabClient = tableau répertoriant les clients connectés
 * - tabThread = tableau des threads associés au traitement de chaque client
 * - tabSalon = tableau répertoriant les salons existants
 * - nbClients = nombre de clients actuellement connectés
 * - dS_fichier = socket de connexion pour le transfert de fichiers
 * - dS = socket de connexion entre les clients et le serveur
 * - portServeur = port sur lequel le serveur est exécuté
 * - semaphoreNbClients = sémaphore pour gérer le nombre de clients
 * - semaphoreThread = sémpahore pour gérer les threads
 * - mutexTabClient = mutexTabClient pour la modification de tabClient[]
 * - mutexSalon = mutexTabSalon pour la modification de tabSalon[]
 */

Client tabClient[MAX_CLIENT];
pthread_t tabThread[MAX_CLIENT];
Salon tabSalon[MAX_SALON];
long nbClient = 0;
int dS_fichier;
int dS;
int portServeur;
sem_t semaphoreNbClients;
sem_t semaphoreThread;
pthread_mutex_t mutexTabClient;
pthread_mutex_t mutexSalon;

// Déclaration des fonctions
int donnerNumClient();
int verifPseudo(char *pseudo);
long pseudoTodSC(char *pseudo);
void envoi(int dS, char *msg, int id);
void envoiATous(char *msg);
void envoiPrive(char *pseudoRecepteur, char *msg);
void reception(int dS, char *rep, ssize_t size);
int finDeCommunication(char *msg);
void *copieFichierThread(void *clientIndex);
void *envoieFichierThread(void *clientIndex);
int nbChiffreDansNombre(int nombre);
void endOfThread(int numclient);
int utilisationCommande(char *msg, char *pseudoEnvoyeur);
void *communication(void *clientParam);
void sigintHandler(int sig_num);

/**
 * @brief Fonction pour gérer les indices du tableau de clients.
 *
 * @return un entier, indice du premier emplacement disponible ;
 *         -1 si tout les emplacements sont occupés.
 */
int donnerNumClient()
{
	int i = 0;
	while (i < MAX_CLIENT)
	{
		if (!tabClient[i].estOccupe)
		{
			return i;
		}
		i += 1;
	}
	return -1;
}

/**
 * @brief Fonctions pour vérifier que le pseudo est unique.
 *
 * @param pseudo pseudo à vérifier
 * @return un entier ;
 *         1 si le pseudo existe déjà,
 *         0 si le pseudo n'existe pas.
 */
int verifPseudo(char *pseudo)
{
	int i = 0;
	while (i < MAX_CLIENT)
	{
		if (tabClient[i].estOccupe && strcmp(pseudo, tabClient[i].pseudo) == 0)
		{
			return 1;
		}
		i++;
	}
	return 0;
}

/**
 * @brief Fonction pour récupérer l'index dans tabClient selon un pseudo donné.
 *
 * @param pseudo pseudo pour lequel on cherche l'index du tableau
 * @return index du client nommé [pseudo] ;
 *         -1 si le pseudo n'existe pas.
 */
long pseudoToInt(char *pseudo)
{
	int i = 0;
	while (i < MAX_CLIENT)
	{
		if (tabClient[i].estOccupe && strcmp(tabClient[i].pseudo, pseudo) == 0)
		{
			return i;
		}
		i++;
	}
	return -1;
}

/**
 * @brief Envoie un message à toutes les sockets présentes dans le tableau des clients pour un même idSalon
 * et teste que tout se passe bien.
 *
 * @param dS expéditeur du message
 * @param msg message à envoyer
 * @param idSalon id du salon sur lequel envoyé le message
 */
void envoi(int dS, char *msg, int idSalon)
{
	for (int i = 0; i < MAX_CLIENT; i++)
	{
		// On n'envoie pas au client qui a écrit le message
		if (tabClient[i].estOccupe && dS != tabClient[i].dSC && idSalon == tabClient[i].idSalon && strcmp(tabClient[i].pseudo, " ") != 0)
		{
			if (send(tabClient[i].dSC, msg, strlen(msg) + 1, 0) == -1)
			{
				perror("Erreur au send");
				exit(-1);
			}
		}
	}
}

/**
 * @brief Envoie un message à toutes les sockets présentes dans le tableau des clients
 * et teste que tout se passe bien.
 *
 * @param msg message à envoyer
 */
void envoiATous(char *msg)
{
	for (int i = 0; i < MAX_CLIENT; i++)
	{
		// On n'envoie pas au client qui a écrit le message
		if (tabClient[i].estOccupe)
		{
			if (send(tabClient[i].dSC, msg, strlen(msg) + 1, 0) == -1)
			{
				perror("Erreur à l'envoi à tout le monde");
				exit(-1);
			}
		}
	}
}

/**
 * @brief Envoie un message en privé à un client en particulier
 * et teste que tout se passe bien.
 *
 * @param pseudoRecepteur destinataire du message
 * @param msg message à envoyer
 */
void envoiPrive(char *pseudoRecepteur, char *msg)
{
	int i = pseudoToInt(pseudoRecepteur);
	if (i == -1)
	{
		perror("Pseudo pas trouvé");
		exit(-1);
	}
	long dSC = tabClient[i].dSC;
	if (send(dSC, msg, strlen(msg) + 1, 0) == -1)
	{
		perror("Erreur à l'envoi du mp");
		exit(-1);
	}
}

/**
 * @brief Receptionne un message d'une socket et teste que tout se passe bien.
 *
 * @param dS socket sur laquelle recevoir
 * @param rep buffer où stocker le message reçu
 * @param size taille maximum du message à recevoir
 */
void reception(int dS, char *rep, ssize_t size)
{
	if (recv(dS, rep, size, 0) == -1)
	{
		perror("Erreur à la réception");
		exit(-1);
	}
}

/**
 * @brief Fonction pour vérifier si un client souhaite quitter la communication.
 *
 * @param msg message du client à vérifier
 * @return 1 si le client veut quitter, 0 sinon.
 */
int finDeCommunication(char *msg)
{
	if (strcmp(msg, "/fin\n") == 0)
	{
		strcpy(msg, "** a quitté la communication **\n");
		return 1;
	}
	return 0;
}

/**
 * @brief Permet de savoir la longueur d'un chiffre.
 *
 * @param nombre le nombre dont on souhaite connaitre la taille
 * @return le nombre de chiffre qui compose ce nombre.
 */
int nbChiffreDansNombre(int nombre)
{
	int nbChiffre = 0;
	while (nombre > 0)
	{
		nombre = nombre / 10;
		nbChiffre += 1;
	}
	return nbChiffre;
}

/**
 * @brief Permet de join les threads terminés.
 *
 * @param numclient indice du thread à join
 */
void endOfThread(int numclient)
{
	pthread_join(tabThread[numclient], 0);
	sem_post(&semaphoreThread);
}

/**
 * @brief Vérifie si un client souhaite utiliser une des commandes
 * disponibles.
 *
 * @param msg message du client à vérifier
 * @param pseudoEnvoyeur pseudo du client qui envoie le message
 *
 * @return 1 si le client utilise une commande, 0 sinon.
 */
int utilisationCommande(char *msg, char *pseudoEnvoyeur)
{
	char *strToken = strtok(msg, " ");
	if (strcmp(strToken, "/estConnecte") == 0)
	{
		// Récupération du pseudo
		char *pseudoAVerif = (char *)malloc(sizeof(char) * TAILLE_PSEUDO);
		pseudoAVerif = strtok(NULL, " ");
		pseudoAVerif = strtok(pseudoAVerif, "\n");

		// Préparation du message à envoyer
		char *msgAEnvoyer = (char *)malloc(sizeof(char) * (TAILLE_PSEUDO + 20));
		strcat(msgAEnvoyer, pseudoAVerif);

		if (verifPseudo(pseudoAVerif))
		{
			// Envoi du message au destinataire
			strcat(msgAEnvoyer, " est en ligne");
			envoiPrive(pseudoEnvoyeur, msgAEnvoyer);
		}
		else
		{
			// Envoi du message au destinataire
			strcat(msgAEnvoyer, " n'est pas en ligne\n");
			envoiPrive(pseudoEnvoyeur, msgAEnvoyer);
		}

		free(msgAEnvoyer);
		return 1;
	}
	else if (strcmp(strToken, "/aide") == 0)
	{
		// Envoie de l'aide au client, un message par ligne
		FILE *fichierCom = NULL;
		fichierCom = fopen("commande.txt", "r");

		fseek(fichierCom, 0, SEEK_END);
		int longueur = ftell(fichierCom);
		fseek(fichierCom, 0, SEEK_SET);

		if (fichierCom != NULL)
		{
			char *toutFichier = (char *)malloc(longueur);
			fread(toutFichier, sizeof(char), longueur, fichierCom);

			envoiPrive(pseudoEnvoyeur, toutFichier);

			free(toutFichier);
		}
		else
		{
			// On affiche un message d'erreur si le fichier n'a pas réussi a être ouvert
			printf("Impossible d\'ouvrir le fichier de commande pour l\'aide");
		}
		fclose(fichierCom);
		return 1;
	}
	else if (strcmp(strToken, "/enLigne") == 0)
	{
		char *chaineEnLigne = malloc(sizeof(char) * (TAILLE_PSEUDO + 15) * 20); // Tous les 20 utilisateurs envoie de la chaine concaténée
		int compteur = 0;

		for (int i = 0; i < MAX_CLIENT; i++)
		{
			if (tabClient[i].estOccupe)
			{
				compteur++;
				char *msgAEnvoyer = (char *)malloc(sizeof(char) * (TAILLE_PSEUDO + 15));
				strcpy(msgAEnvoyer, tabClient[i].pseudo);
				strcat(msgAEnvoyer, " est en ligne\n"); // Taille du message 15

				strcat(chaineEnLigne, msgAEnvoyer);

				free(msgAEnvoyer);
			}
			if (compteur == 20)
			{
				envoiPrive(pseudoEnvoyeur, chaineEnLigne);
				strcpy(chaineEnLigne, "");
				compteur = 0;
			}
		}
		if (compteur != 0)
		{
			envoiPrive(pseudoEnvoyeur, chaineEnLigne);
		}
		free(chaineEnLigne);

		return 1;
	}
	else if (strToken[0] == '/')
	{
		envoiPrive(pseudoEnvoyeur, "Faites \"/aide\" pour avoir accès aux commandes disponibles et leur fonctionnement\n");
		return 1;
	}

	return 0;
}

/**
 * @brief Fonction principale de communication entre un
 * client et le serveur.
 *
 * @param clientParam numéro du client en question
 */
void *communication(void *clientParam)
{

	int numClient = (long)clientParam;

	// Réception du pseudo
	char *pseudo = (char *)malloc(sizeof(char) * (TAILLE_PSEUDO + 29)); // voir Ligne 1330
	reception(tabClient[numClient].dSC, pseudo, sizeof(char) * TAILLE_PSEUDO);
	pseudo = strtok(pseudo, "\n");

	while (pseudo == NULL || verifPseudo(pseudo))
	{
		send(tabClient[numClient].dSC, "Pseudo déjà existant\n", strlen("Pseudo déjà existant\n") + 1, 0);
		reception(tabClient[numClient].dSC, pseudo, sizeof(char) * TAILLE_PSEUDO);
		pseudo = strtok(pseudo, "\n");
	}

	tabClient[numClient].pseudo = (char *)malloc(sizeof(char) * TAILLE_PSEUDO);
	strcpy(tabClient[numClient].pseudo, pseudo);
	tabClient[numClient].idSalon = 0;

	// On envoie un message pour dire au client qu'il est bien connecté
	char *repServ = (char *)malloc(sizeof(char) * 61);
	repServ = "Entrer /aide pour avoir la liste des commandes disponibles\n"; // 61
	envoiPrive(pseudo, repServ);

	// On vérifie que ce n'est pas le pseudo par défaut
	if (strcmp(pseudo, "FinClient") != 0)
	{
		// On envoie un message pour avertir les autres clients de l'arrivée du nouveau client
		strcat(pseudo, " a rejoint la communication\n"); // 29
		envoi(tabClient[numClient].dSC, pseudo, 0);
	}

	// On a un client en plus sur le serveur, on incrémente
	pthread_mutex_lock(&mutexTabClient);
	nbClient += 1;
	pthread_mutex_unlock(&mutexTabClient);

	printf("Clients connectés : %ld\n", nbClient);

	int estFin = 0;
	char *pseudoEnvoyeur = tabClient[numClient].pseudo;

	while (!estFin)
	{
		// Réception du message
		char *msgReceived = (char *)malloc(sizeof(char) * TAILLE_MESSAGE);
		reception(tabClient[numClient].dSC, msgReceived, sizeof(char) * TAILLE_MESSAGE);
		printf("\nMessage recu: %s \n", msgReceived);

		// On verifie si le client veut terminer la communication
		estFin = finDeCommunication(msgReceived);

		// On vérifie si le client utilise une des commandes
		char *msgToVerif = (char *)malloc(sizeof(char) * strlen(msgReceived));
		strcpy(msgToVerif, msgReceived);

		if (utilisationCommande(msgToVerif, pseudoEnvoyeur))
		{
			free(msgReceived);
			continue;
		}

		// Ajout du pseudo de l'expéditeur devant le message à envoyer
		char *msgAEnvoyer = (char *)malloc(sizeof(char) * (TAILLE_PSEUDO + 4 + strlen(msgReceived)));
		strcpy(msgAEnvoyer, pseudoEnvoyeur);
		strcat(msgAEnvoyer, " : ");
		strcat(msgAEnvoyer, msgReceived);
		free(msgReceived);

		// Envoi du message aux autres clients
		printf("Envoi du message aux %ld clients. \n", nbClient - 1);
		envoi(tabClient[numClient].dSC, msgAEnvoyer, tabClient[numClient].idSalon);

		free(msgAEnvoyer);
	}

	// Fermeture du socket client
	pthread_mutex_lock(&mutexTabClient);
	nbClient = nbClient - 1;
	tabClient[numClient].estOccupe = 0;
	free(tabClient[numClient].pseudo);
	pthread_mutex_unlock(&mutexTabClient);

	shutdown(tabClient[numClient].dSC, 2);

	// On relache le sémaphore pour les clients en attente
	sem_post(&semaphoreNbClients);

	// On incrémente le sémaphore des threads
	sem_wait(&semaphoreThread);
	endOfThread(numClient);

	return NULL;
}

/**
 * @brief Fonction gérant l'interruption du programme par CTRL+C
 * Correspond à la gestion des signaux.
 *
 * @param sig_num numéro du signal
 */
void sigintHandler(int sig_num)
{
	printf("\nFin du serveur\n");
	if (dS != 0)
	{
		envoiATous("LE SERVEUR S'EST MOMENTANEMENT ARRETE, DECONNEXION...\n");
		envoiATous("Tout ce message est le code secret pour désactiver les clients");

		int i = 0;
		while (i < MAX_CLIENT)
		{
			if (tabClient[i].estOccupe)
			{
				endOfThread(i);
			}
			i += 1;
		}

		shutdown(dS, 2);
		sem_destroy(&semaphoreNbClients);
		sem_destroy(&semaphoreThread);
		pthread_mutex_destroy(&mutexTabClient);
		pthread_mutex_destroy(&mutexSalon);
	}

	exit(1);
}

/*
 * _____________________ MAIN _____________________
 */
// argv[1] = port

int main(int argc, char *argv[])
{
	// Verification du nombre de paramètres
	if (argc < 2)
	{
		perror("Erreur : Lancez avec ./serveur [votre_port] ");
		exit(-1);
	}

	printf("Début programme\n");

	portServeur = atoi(argv[1]);

	// Fin avec Ctrl + C
	signal(SIGINT, sigintHandler);

	// Création du salon général de discussion
	tabSalon[0].idSalon = 0;
	tabSalon[0].estOccupe = 1;
	tabSalon[0].nom = "Chat_général";
	tabSalon[0].description = "Salon général par défaut";
	tabSalon[0].nbPlace = MAX_CLIENT;

	// Création de la socket
	dS = socket(PF_INET, SOCK_STREAM, 0);
	if (dS < 0)
	{
		perror("Problème de création de socket serveur");
		exit(-1);
	}
	printf("Socket Créé\n");

	// Nommage de la socket
	struct sockaddr_in ad;
	ad.sin_family = AF_INET;
	ad.sin_addr.s_addr = INADDR_ANY;
	ad.sin_port = htons(atoi(argv[1]));

	if (bind(dS, (struct sockaddr *)&ad, sizeof(ad)) < 0)
	{
		perror("Erreur lors du nommage de la socket");
		exit(-1);
	}
	printf("Socket nommée\n");

	// Initialisation du sémaphore pour gérer les clients
	sem_init(&semaphoreNbClients, PTHREAD_PROCESS_SHARED, MAX_CLIENT);

	// Initialisation du sémaphore pour gérer les threads
	sem_init(&semaphoreThread, PTHREAD_PROCESS_SHARED, 1);

	// Passage de la socket en mode écoute
	if (listen(dS, 7) < 0)
	{
		perror("Problème au niveau du listen");
		exit(-1);
	}
	printf("Mode écoute\n");

	while (1)
	{
		// Vérifions si on peut accepter un client
		// On attends la disponibilité du sémaphore
		sem_wait(&semaphoreNbClients);

		// Acceptons une connexion
		struct sockaddr_in aC;
		socklen_t lg = sizeof(struct sockaddr_in);
		int dSC = accept(dS, (struct sockaddr *)&aC, &lg);
		if (dSC < 0)
		{
			perror("Problème lors de l'acceptation du client\n");
			exit(-1);
		}
		printf("Client %ld connecté\n", nbClient);

		// Enregistrement du client
		pthread_mutex_lock(&mutexTabClient);
		long numClient = donnerNumClient();
		tabClient[numClient].estOccupe = 1;
		tabClient[numClient].dSC = dSC;
		tabClient[numClient].pseudo = malloc(sizeof(char) * TAILLE_PSEUDO);
		strcpy(tabClient[numClient].pseudo, " ");
		pthread_mutex_unlock(&mutexTabClient);

		//_____________________ Communication _____________________
		if (pthread_create(&tabThread[numClient], NULL, communication, (void *)numClient) == -1)
		{
			perror("Erreur thread create");
		}
	}
	// ############  	N'arrive jamais  	####################
	shutdown(dS, 2);
	sem_destroy(&semaphoreNbClients);
	sem_destroy(&semaphoreThread);
	pthread_mutex_destroy(&mutexTabClient);
	printf("Fin du programme\n");
	// #########################################################
}
