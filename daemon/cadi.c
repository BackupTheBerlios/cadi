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
#include "cadi.h"


/** L'adresse du serveur */
static in_addr_t server_in_addr;

/** Le port de connection sur le serveur */
static unsigned port;

/**
 * Affiche l'aide du client.
 *
 * @param prog nom du programme (argv[0])
 */
static void usage(const char *prog)
{
    puts(CLIENT_VERSION "\n");
    printf("Syntaxe : %s [ -h | -v | -s adresse_serveur [ -p port ] ]\n", prog);
    puts("\t-s adresse_serveur . . l'adresse du serveur où se connecter (défaut: localhost)");
    puts("\t-p port  . . . . . . . le port sur lequel se connecter (défaut: " DEFAULT_PORT_STRING ")");
    puts("\t-v . . . . . . . . . . afficher la version du client");
    puts("\t-h . . . . . . . . . . afficher cette aide");
}

/**
 * Parse la ligne de commande.
 */
static void parse_command_line(int argc, char *argv[])
{
  char *prog = argv[0];
  argc = argc; /* Evite un warning */

  server_in_addr = htonl(INADDR_ANY);
  port = DEFAULT_PORT;

  while (*++argv) {
    /* Version */
    if (!strcmp(*argv, "-v")) { puts(CLIENT_VERSION); exit(EXIT_SUCCESS); }

    /* Aide */
    else if (!strcmp(*argv, "-h")) { usage(prog); exit(EXIT_SUCCESS); }
    
    /* Port */
    else if (!strcmp(*argv, "-p")) {
      if (*(argv + 1) == NULL) { usage(prog); exit(EXIT_FAILURE); }
      port = atoi(*++argv);
    }

    /* Serveur */
    else if (!strcmp(*argv, "-s")) {
      struct hostent *h;
      if (*(argv + 1) == NULL) { usage(prog); exit(EXIT_FAILURE); }
      if (!(h = gethostbyname(*++argv))) {
	herror("Impossible de résoudre l'adresse du serveur");
	exit(EXIT_FAILURE);
      }
      server_in_addr = ((struct in_addr *) h->h_addr_list[0])->s_addr;
    }

    /* Option inconnue */
    else {
      fprintf(stderr, "Option \"%s\" inconnue\n\n", *argv);
      usage(prog);
      exit(EXIT_FAILURE);
    }
  }
}

/**
 * Entrée du programme.
 */
int main(int argc, char *argv[])
{
    struct sockaddr_in server_address;  /* Structure pour la config de la connection au serveur */
    int server_socket;                  /* Le socket connecté au serveur */
    char buffer[MESSAGE_BUFFER_SIZE];   /* Le buffer des messages */
    pid_t pid;

    /* Parse la ligne de commande */
    parse_command_line(argc, argv);

    /* On crée un socket pour se connecter sur un serveur */
    if ((server_socket = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Impossible d'ouvrir de socket");
        return EXIT_FAILURE;
    }

    /* On initialise la structure pour savoir où/comment se connecter */
    memset(&server_address, 0, sizeof server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = server_in_addr;
    server_address.sin_port = htons(port);

    /* On se connecte */
    if (connect(server_socket, (struct sockaddr *) &server_address,
            sizeof server_address) == -1) {
        perror("Impossible de se connecter");
	if (close(server_socket) == -1)
	  perror("Impossible de fermer la connection au serveur");
        return EXIT_FAILURE;
    }

    /* On fork pour l'écoute et l'envoi */
    pid = fork();

    /* Erreur */
    if (pid < 0) {
      perror("Impossible de forker");
      return EXIT_FAILURE;
    }

    if (pid == 0) {
      
      /**********************************************************************/
      /* Dans le processus fils qui permet d'envoyer des données au serveur */
      /**********************************************************************/

      /* A chaque saisie, on envoie au serveur */      
      while (fgets(buffer, MESSAGE_BUFFER_SIZE, stdin)) {
	
	char *p; /* Suppression du '\n' final */
	if ((p = strrchr(buffer, '\n')) != NULL)
	  *p = '\0';

	p = buffer; /* On saute les espaces */
	while (isspace(*p))
	  p++;
	
	/* On envoie avec le '\0' */
	if (send(server_socket, p, strlen(p) + 1, 0) < 0)
	  perror("Impossible d'envoyer le message au serveur");
      }

    } else {

      /********************************************************************/
      /* Dans le processus père qui affiche les messages issus du serveur */
      /* Important de le mettre en père car il peut tuer son fils         */
      /********************************************************************/

      /* On reçoit avec les '\0' */
      while (recv(server_socket, buffer, MESSAGE_BUFFER_SIZE, 0) > 0) {
	int i;
	fprintf(stdout, "réception ...\n");
	for (i = 0; i < 10; i++)
	  printf("%c[%d] ", buffer[i], buffer[i]);
	putchar(10);
	
	/* On affiche le message reçu */
	fprintf(stdout, "%s", buffer);
	fflush(stdout);
	
	/* Le serveur renvoie OK QUIT si il a reçu QUIT.
	   On préférera cela pour quitter proprement, plutôt que d'envoyer QUIT au serveur
	   et de quitter côté client sans attendre de réponse. */
	if (sscanf(buffer, "OK %s", buffer) == 1 && !strcmp(buffer, DETAIL_RET_QUIT))
	  break;
      }
      
    }

    /* Fermeture de la connection */
    if (close(server_socket) == -1) {
      perror("Impossible de fermer la connection au serveur");
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
