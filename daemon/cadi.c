#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <netdb.h>
#include <ctype.h>

#include "cadid.h"

#define CLIENT_VERSION "CaDi Client v0.1a\n"
#define CLIENT_PROMPT  ">> "
#define SERVER_PROMPT  "## "

static in_addr_t server_in_addr;
static unsigned port;

/**
 * Affiche l'aide de CaDi.
 *
 * @param prog nom du programme (argv[0])
 */
static void usage(const char *prog)
{
    puts(CLIENT_VERSION);
    printf("Syntaxe : %s < -v | adresse_serveur > [port]\n", prog);
    puts("-v\tVersion du client");
    fflush(stdout);
}

/**
 * Affiche une erreur à la printf en rajoutant un préfixe.
 *
 * @param format format (à la printf)
 * @param ... arguments
 */
static void ERROR(const char *format, ...)
{
    static char err_msg[MAX_ERR_SIZE];
    va_list args;

    strcpy(err_msg, "ERREUR : ");
    strncat(err_msg, format, MAX_ERR_SIZE - strlen(err_msg) - 1);

    va_start(args, format);
    vfprintf(stderr, err_msg, args);
    putchar('\n');
    fflush(stdout);
}

/**
 * Parse la ligne de commande.
 */
static void parse_command_line(int argc, char *argv[])
{
    struct hostent *h;

    /* Cas foireux */
    if (argc > 3) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Config par défaut */
    if (argc == 1) {
        server_in_addr = htonl(INADDR_ANY);
        port = SERVER_PORT;
        return;
    }

    /* Au moins l'adresse ou la version spécifiée */
    if (argc > 1) {

        /* Version */
        if (!strcmp(argv[1], "-v")) {
            printf(CLIENT_VERSION);
            exit(EXIT_SUCCESS);
        }

        /* Sinon on conclue que c'est l'adresse du serveur */
        if (!(h = gethostbyname(argv[1]))) {
            herror("Impossible de résoudre l'adresse du serveur");
            exit(EXIT_FAILURE);
        }

        /* Le bon cast d'âne trouvé, mouahah */
        server_in_addr = ((struct in_addr *) h->h_addr_list[0])->s_addr;
    }

    /* Il y a un numéro de port également */
    if (argc > 2) {
      int i;

      for (i = 0; argv[2][i] != '\0'; i++)
	if (!isdigit(*argv[2])) {
	  ERROR("Le port doit être numérique.");
	  exit(EXIT_FAILURE);
	}
      
      port = atoi(argv[2]);
    } else {
      port = SERVER_PORT;     /* Option port non précisé, on prend le défaut */
    }

}

/**
 * Entrée du programme.
 */
int main(int argc, char *argv[])
{
    struct sockaddr_in server_address;  /* Structure pour la config de la connection au serveur */
    int server_socket;                  /* Le socket connecté au serveur */
    char buffer[MESSAGE_BUFFER_SIZE];   /* Le buffer des messages // */
    pid_t pid;

    /* Vérifie la ligne de commande */
    parse_command_line(argc, argv);

    /* On crée un socket pour se connecter sur un serveur */
    if ((server_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Impossible d'ouvrir de socket");
        return EXIT_FAILURE;
    }

    /* On initialise la structure pour savoir où/comment se connecter */
    memset(&server_address, 0, sizeof server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = server_in_addr;
    server_address.sin_port = htons(port);

    /* On se connecte */
    if (connect(server_socket, (struct sockaddr *) &(server_address),
            sizeof server_address) < 0) {
        perror("Impossible de se connecter");
        return EXIT_FAILURE;
    }

    /* On fork pour l'écoute et l'envoi */
    pid = fork();

    if (pid < 0) {
      perror("Impossible de forker");
      return EXIT_FAILURE;
    }

    if (pid == 0) {
      
      /**********************************************************************/
      /* Dans le processus fils qui permet d'envoyer des données au serveur */
      /**********************************************************************/

      printf(CLIENT_PROMPT);
      fflush(stdout);

      /* A chaque saisie, on envoie au serveur */      
      while (fgets(buffer, MESSAGE_BUFFER_SIZE, stdin)) {
	
	char *p; /* Suppression du '\n' final */
	if ((p = strrchr(buffer, '\n')) != NULL)
	  *p = '\0';

	p = buffer; /* On saute les espaces */
	while (isspace(*p))
	  p++;
	
	if (strlen(p) == 0) {
	  printf(CLIENT_PROMPT);
	  continue;
	}

	if (send(server_socket, p, strlen(p) + 1, 0) < 0) /* On envoie le '\0' avec */
	  perror("Impossible d'envoyer de message au serveur");
      }

    } else {

      /********************************************************************/
      /* Dans le processus père qui affiche les messages issus du serveur */
      /* Important de le mettre en père car il peut tuer son fils         */
      /********************************************************************/

      while (recv(server_socket, buffer, MESSAGE_BUFFER_SIZE, 0) > 0) {
	/* Petit hack pour virer le prompt au début, quand on recoit le msg de bienvenue */
	unsigned i;
	for (i = 0; i < sizeof(CLIENT_PROMPT) - 1; i++)
	       printf("\b"); 

	/* On affiche le message reçu */
        printf(SERVER_PROMPT "%s\n", buffer);
	
	/* Le serveur renvoie OK QUIT si il a reçu QUIT.
	   On préférera cela pour quitter proprement, plutôt que d'envoyer QUIT au serveur
	   et de quitter côté client sans attendre de réponse. */
	if (sscanf(buffer, "OK %s", buffer) == 1 && !strcmp(buffer, DETAIL_RET_QUIT))
	  break;

	printf(CLIENT_PROMPT);
	fflush(stdout);
      }
      
    }

    close(server_socket);
    return EXIT_SUCCESS;
}
