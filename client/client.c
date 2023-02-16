#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <SDL.h>
#include <SDL2/SDL_ttf.h>

/**
 * Définition des différents codes pour l'utilisation de couleurs dans le texte
 */
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

/**
 * - TAILLE_PSEUDO = taille maximum du pseudo
 * - TAILLE_MESSAGE = taille maximum d'un message
 * - WINDOW_WIDTH = taille de la fenêtre en largeur
 * - WINDOW_HEIGHT = taille de la fenêtre en hauteur
 */
#define TAILLE_PSEUDO 20
#define TAILLE_MESSAGE 500
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

/**
 * - nomFichier = nom du fichier à transférer
 * - estFin = booléen vérifiant si le client est connecté ou s'il a terminé la discussion avec le serveur
 * - dS = socket du serveur
 * - boolConnect = booléen vérifiant si le client est connecté afin de gérer les signaux (CTRL+C)
 * - addrServeur = adresse du serveur sur laquelle est connecté le client
 * - portServeur = port du serveur sur lequel est connecté le client
 * - aS = structure contenant toutes les informations de connexion du client au serveur
 * - thread_envoi = thread gérant l'envoi de messages
 * - thread_reception = thread gérant la réception de messages
 */
char nomFichier[20];
int estFin = 0;
int dS = -1;
int boolConnect = 0;
char *addrServeur;
int portServeur;
struct sockaddr_in aS;

// Création des threads
pthread_t thread_envoi;
pthread_t thread_reception;

// Déclaration des fonctions
int finDeCommunication(char *msg);
void envoi(char *msg);
void *envoieFichier();
void *receptionFichier(void *ds);
int utilisationCommande(char *msg);
void *envoiPourThread();
void reception(char *rep, ssize_t size);
void *receptionPourThread();
void sigintHandler(int sig_num);
void SDL_ExitWithError(const char *message);

/**
 * @brief Vérifie si un client souhaite quitter la communication.
 *
 * @param msg message du client à vérifier
 * @return 1 si le client veut quitter, 0 sinon.
 */
int finDeCommunication(char *msg)
{
	if (strcmp(msg, "/fin\n") == 0)
	{
		return 1;
	}
	return 0;
}

/**
 * @brief Envoie un message au serveur et teste que tout se passe bien.
 *
 * @param msg message à envoyer
 */
void envoi(char *msg)
{
	if (send(dS, msg, strlen(msg) + 1, 0) == -1)
	{
		fprintf(stderr, ANSI_COLOR_RED "Votre message n'a pas pu être envoyé\n" ANSI_COLOR_RESET);
		return;
	}
}

/**
 * @brief Fonction principale pour le thread gérant l'envoi de messages.
 */
void *envoiPourThread()
{
	while (!estFin)
	{
		/*Saisie du message au clavier*/
		char *m = (char *)malloc(sizeof(char) * TAILLE_MESSAGE);
		fgets(m, TAILLE_MESSAGE, stdin);

		// On vérifie si le client veut quitter la communication
		estFin = finDeCommunication(m);

		// On vérifie si le client utilise une des commandes
		char *msgAVerif = (char *)malloc(sizeof(char) * strlen(m));
		strcpy(msgAVerif, m);


		// Envoi
		envoi(m);
		free(m);
	}
	shutdown(dS, 2);
	return NULL;
}

/**
 * @brief Réceptionne un message du serveur et teste que tout se passe bien.
 *
 * @param rep buffer contenant le message reçu
 * @param size taille maximum du message à recevoir
 */
void reception(char *rep, ssize_t size)
{
	if (recv(dS, rep, size, 0) == -1)
	{
		printf(ANSI_COLOR_YELLOW "** fin de la communication **\n" ANSI_COLOR_RESET);
		exit(-1);
	}
}

/**
 * @brief Fonction principale pour le thread gérant la réception de messages.
 */
void *receptionPourThread()
{
	while (!estFin)
	{
		char *r = (char *)malloc(sizeof(char) * TAILLE_MESSAGE);
		reception(r, sizeof(char) * TAILLE_MESSAGE);
		if (strcmp(r, "Tout ce message est le code secret pour désactiver les clients") == 0)
		{
			free(r);
			break;
		}

		printf("%s", r);
		free(r);
	}
	shutdown(dS, 2);
	pthread_cancel(thread_envoi);
	return NULL;
}

/**
 * @brief Fonction gérant l'interruption du programme par CTRL+C.
 * Correspond à la gestion des signaux.
 *
 * @param sig_num numéro du signal
 */
void sigintHandler(int sig_num)
{
	printf(ANSI_COLOR_YELLOW "\nProgramme Fermé\n" ANSI_COLOR_RESET);
	if (!boolConnect)
	{
		char *myPseudoEnd = (char *)malloc(sizeof(char) * 12);
		myPseudoEnd = "FinClient";
		envoi(myPseudoEnd);
	}
	sleep(0.2);
	envoi("/fin\n");
	exit(1);
}

void SDL_ExitWithError(const char *message)
{
	printf(ANSI_COLOR_YELLOW "\n%s\n" ANSI_COLOR_RESET, message);
	sleep(0.2);
	envoi("/fin\n");
	exit(1);
}

// argv[1] = adresse ip
// argv[2] = port
int main(int argc, char *argv[])
{

	if (argc < 3)
	{
		fprintf(stderr, ANSI_COLOR_RED "Erreur : Lancez avec ./client [votre_ip] [votre_port]\n" ANSI_COLOR_RESET);
		return -1;
	}
	printf(ANSI_COLOR_MAGENTA "Début programme\n" ANSI_COLOR_RESET);

	addrServeur = argv[1];
	portServeur = atoi(argv[2]);

	// Création de la socket
	dS = socket(PF_INET, SOCK_STREAM, 0);
	if (dS == -1)
	{
		fprintf(stderr, ANSI_COLOR_RED "Problème de création de socket client\n" ANSI_COLOR_RESET);
		return -1;
	}
	printf(ANSI_COLOR_MAGENTA "Socket Créé\n" ANSI_COLOR_RESET);

	// Nommage de la socket
	aS.sin_family = AF_INET;
	inet_pton(AF_INET, argv[1], &(aS.sin_addr));
	aS.sin_port = htons(atoi(argv[2]));
	socklen_t lgA = sizeof(struct sockaddr_in);

	// Envoi d'une demande de connexion
	printf(ANSI_COLOR_MAGENTA "Connexion en cours...\n" ANSI_COLOR_RESET);
	if (connect(dS, (struct sockaddr *)&aS, lgA) < 0)
	{
		fprintf(stderr, ANSI_COLOR_RED "Problème de connexion au serveur\n" ANSI_COLOR_RESET);
		exit(-1);
	}
	printf(ANSI_COLOR_MAGENTA "Socket connectée\n" ANSI_COLOR_RESET);

	// Fin avec Ctrl + C
	signal(SIGINT, sigintHandler);

	// Saisie du pseudo du client au clavier
	char *monPseudo = (char *)malloc(sizeof(char) * TAILLE_PSEUDO);
	do
	{
		printf(ANSI_COLOR_MAGENTA "Votre pseudo (maximum 19 caractères):\n" ANSI_COLOR_RESET);
		fgets(monPseudo, TAILLE_PSEUDO, stdin);
		for (int i = 0; i < strlen(monPseudo); i++)
		{
			if (monPseudo[i] == ' ')
			{
				monPseudo[i] = '_';
			}
		}
	} while (strcmp(monPseudo, "\n") == 0);

	// Envoie du pseudo
	envoi(monPseudo);

	char *repServeur = (char *)malloc(sizeof(char) * 61);
	// Récéption de la réponse du serveur
	reception(repServeur, sizeof(char) * 61);
	printf(ANSI_COLOR_MAGENTA "%s\n" ANSI_COLOR_RESET, repServeur);

	while (strcmp(repServeur, "Pseudo déjà existant\n") == 0)
	{
		// Saisie du pseudo du client au clavier
		printf(ANSI_COLOR_MAGENTA "Votre pseudo (maximum 19 caractères):\n" ANSI_COLOR_RESET);
		fgets(monPseudo, TAILLE_PSEUDO, stdin);

		for (int i = 0; i < strlen(monPseudo); i++)
		{
			if (monPseudo[i] == ' ')
			{
				monPseudo[i] = '_';
			}
		}

		// Envoie du pseudo
		envoi(monPseudo);

		// Récéption de la réponse du serveur
		reception(repServeur, sizeof(char) * 61);
		printf(ANSI_COLOR_MAGENTA "%s\n" ANSI_COLOR_RESET, repServeur);

	}

	free(monPseudo);
	printf(ANSI_COLOR_MAGENTA "Connexion complète\n" ANSI_COLOR_RESET);
	boolConnect = 1;

	/*----------------------------------------------------------------------------------------------------------------------------------*/

	SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    TTF_Font *font = NULL;

    //Fonction si SDL ne démarre pas > Log erreur
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
            SDL_ExitWithError("Initialisation SDL");

    if (TTF_Init() != 0)
            SDL_ExitWithError("Initialisation TTF");

    //Execution du programme....
    //SDL_CreateWindow("Titre de la fenêtre", Position X, Position Y, largeur, hauteur, affichage)
    window = SDL_CreateWindow("Première fenêtre SDL 2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);

    if (window == NULL)
            SDL_ExitWithError("Impossible de creer la fenêtre echouee");

	/*----------------------------------------------------------------------------------------------------------------------------------*/

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    SDL_Surface *image = NULL;
    SDL_Texture *texture = NULL;

    //SDL_LoadBMP(chemin approximatif de l'image en bmp)
    image = SDL_LoadBMP("Image/planet2.bmp");

    if (image == NULL)
    {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_ExitWithError("Impossible de charger l'image");
    }

    // /*
    // Permet la création de la future texture
    // SDL_CreateTextureFromSurface(var rendu, var image)
    // */
    // texture = SDL_CreateTextureFromSurface(renderer, image);
    // //Permet de libérer l'espace utiliser par le chargement de l'image
    // SDL_FreeSurface(image);

    // if (texture == NULL)
    // {
    //     SDL_DestroyRenderer(renderer);
    //     SDL_DestroyWindow(window);
    //     // SDL_ExitWithError("Impossible de creer la texture");
    // }

    // SDL_Rect rectangle;

    // /*
    // SDL_QueryTexture(var texture, NULL, NULL, largeur du rectangle, hauteur du rectangle)
    // Permet de chargée la texture dans la mémoire
    // */
    // if (SDL_QueryTexture(texture, NULL, NULL, &rectangle.w, &rectangle.h) != 0)
    // {
    //     SDL_DestroyRenderer(renderer);
    //     SDL_DestroyWindow(window);
    //     // SDL_ExitWithError("Impossible de charger la texture");
    // }

    // rectangle.x = (WINDOW_WIDTH - rectangle.w) / 2;
    // rectangle.y = (WINDOW_HEIGHT - rectangle.h) / 2;

    // /*
    // Permet l'affichage de la texture
    // SDL_RenderCopy(var rendu, var texture, NULL, var rectangle)
    // */
    // if (SDL_RenderCopy(renderer, texture, NULL, &rectangle) != 0)
    // {
    //     SDL_DestroyRenderer(renderer);
    //     SDL_DestroyWindow(window);
    //     // SDL_ExitWithError("Impossible d'afficher la texture'");
    // }

	SDL_RenderPresent(renderer);

	/*----------------------------------------------------------------------------------------------------------------------------------*/

	SDL_bool program_launched = SDL_TRUE;

    while(program_launched)
    {
        SDL_Event event;

        while(SDL_PollEvent(&event))
        {
            switch(event.type)
            {
                // case SDL_KEYDOWN:
                //     switch(event.key.keysym.sym)
                //     {
                //         case SDLK_b:
                //             printf("Vous avez appuye sur B\n");
                //             continue;

                //         default:
                //             continue;
                //     }

                case SDL_MOUSEBUTTONDOWN:

                        /*
                            SDL_BUTTON_LEFT
                            SDL_BUTTON_MIDDLE
                            SDL_BUTTON_RIGHT
                        */

                        if (event.button.button == SDL_BUTTON_LEFT)
                                printf("Clic gauche ! \n");
                        if (event.button.button == SDL_BUTTON_RIGHT)
                                printf("Clic droit ! \n");

                        // if (event.button.clicks >= 2)
                        //     printf("Double Clic !\n");

                        // printf("Clic en %dx/%dy\n", event.button.x, event.button.y);
                        break;

                case SDL_QUIT:
                    program_launched = SDL_FALSE;
					sigintHandler(2);
                    break;
                
                default:
                    break;
            }
        }
    }

	/*----------------------------------------------------------------------------------------------------------------------------------*/

	SDL_DestroyTexture(texture); //##############
    SDL_DestroyRenderer(renderer); //##############
    SDL_DestroyWindow(window); //##############
    // TTF_CloseFont(font);
    // TTF_Quit();
    SDL_Quit(); //##############

	//_____________________ Communication _____________________

	if (pthread_create(&thread_envoi, NULL, envoiPourThread, 0) < 0)
	{
		fprintf(stderr, ANSI_COLOR_RED "Erreur de création de thread d'envoi client\n" ANSI_COLOR_RESET);
		exit(-1);
	}

	if (pthread_create(&thread_reception, NULL, receptionPourThread, 0) < 0)
	{
		fprintf(stderr, ANSI_COLOR_RED "Erreur de création de thread réception client\n" ANSI_COLOR_RESET);
		exit(-1);
	}

	pthread_join(thread_envoi, NULL);
	pthread_join(thread_reception, NULL);

	printf(ANSI_COLOR_YELLOW "Fin du programme\n" ANSI_COLOR_RESET);

	return 1;
}