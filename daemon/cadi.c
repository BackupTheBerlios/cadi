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
#include "config.h"

/** L'adresse du serveur */
static in_addr_t server_in_addr;

/** Le port de connection sur le serveur */
static unsigned port;

/** La version du client */
static const char *client_version = "Doom Client v0.5a";

/**
 * Affiche l'aide du client.
 *
 * @param prog nom du programme (argv[0])
 */
static void usage(const char *prog)
{
  puts(client_version);
  printf("Usage : %s [ -h | -v | -s adresse_serveur [ -p port ] ]\n", prog);
  puts("\t-s adresse_serveur . . l'adresse du serveur o� se connecter (d�faut: localhost)");
  printf("\t-p port  . . . . . . . le port sur lequel se connecter (d�faut: %d)\n", DEFAULT_PORT);
  puts("\t-v . . . . . . . . . . afficher la version du client");
  puts("\t-h . . . . . . . . . . afficher cette aide");
}

/**
 * Parse la ligne de commande.
 */
static void parse_command_line(int argc, char *argv[])
{

  const char *prog = argv[0];
  argc = argc; /* Evite un warning */
  
  /* Par d�faut */
  server_in_addr = htonl(INADDR_ANY);
  port = DEFAULT_PORT;

  /* On traite tous les arguments */
  while (*++argv)
    {
      /* Version */
      if (!strcmp(*argv, "-v"))
	{
	  puts(client_version);
	  exit(EXIT_SUCCESS);
	}
      
      /* Aide */
      else if (!strcmp(*argv, "-h"))
	{
	  usage(prog);
	  exit(EXIT_SUCCESS);
	}

      /* Port */
      else if (!strcmp(*argv, "-p"))
	{
	  if (*(argv + 1) == NULL)
	    {
	      usage(prog);
	      exit(EXIT_FAILURE);
	    }
	  port = atoi(*++argv);
	}

      /* Serveur */
      else if (!strcmp(*argv, "-s"))
	{
	  if (*(argv + 1) == NULL)
	    {
	      usage(prog);
	      exit(EXIT_FAILURE);
	    }

	  struct hostent *h;
	  if (!(h = gethostbyname(*++argv)))
	    {
	      herror("gethostbyname");
	      exit(EXIT_FAILURE);
	    }

	  server_in_addr = ((struct in_addr *) h->h_addr_list[0])->s_addr;
	}
      
      /* Option inconnue */
      else
	{
	  fprintf(stderr, "Option \"%s\" inconnue\n\n", *argv);
	  usage(prog);
	  exit(EXIT_FAILURE);
	}
    }
}

/**
 * Point d'entr�e du programme.
 */
int main(int argc, char *argv[])
{
  parse_command_line(argc, argv);
  
  /* On cr�e un socket pour se connecter sur un serveur */
  int server_socket;
  if ((server_socket = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror("socket");
      return EXIT_FAILURE;
    }
  
  /* On initialise la structure pour savoir o�/comment se connecter */
  struct sockaddr_in server_address;
  memset(&server_address, 0, sizeof server_address);
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = server_in_addr; /* Adresse de connection */
  server_address.sin_port = htons(port); /* Le port */

  /* On se connecte */
  if (connect(server_socket, (struct sockaddr *) &server_address, sizeof server_address) == -1)
    {
      perror("connect");
      if (close(server_socket) == -1)
	perror("close");
      return EXIT_FAILURE;
    }

  /* Buffer qui contiendra les messages serveur -> client, et client -> serveur (fork�) */
  char buffer[MESSAGE_BUFFER_SIZE];

  /* On fork pour l'�coute et l'envoi */
  switch (fork()) {

  case -1: /* Erreur */
    {
      perror("fork");
      break;
    }

  case 0: /* Fils -- Envoie au serveur les messages tap�s par le client */
    {
      
      /* A chaque saisie, on envoie au serveur */
      while (fgets(buffer, MESSAGE_BUFFER_SIZE, stdin)) {
	
	/* On se place en fin de cha�ne */
	char *p = buffer + strlen(buffer) - 1; 

	while (isspace(*p)) /* Suppression des trailing spaces */
	  *p-- = '\0';
	
	p = buffer; /* D�calage du d�but de cha�ne jusque le premier non-espace */
	while (isspace(*p))
	  p++;
	
	/* On envoie avec un '\0' */
	if (send(server_socket, p, strlen(p) + 1, 0) < 0)
	  perror("send");
      }

      break; /* Lors d'un Ctrl+D */
    }

  default: /* P�re -- Affiche les messages issus du serveur */
    {
      
      /* Important de le mettre en p�re car il peut tuer son fils (fgets bloquant sinon) */
      
      int i;
      
      while ((i = recv(server_socket, buffer, MESSAGE_BUFFER_SIZE - 1, 0)) > 0)
	{
	  /* On ne voudrait pas afficher toute la m�moire quand m�me */
	  buffer[i] = '\0';
	  
	  /* On affiche le message re�u */
	  printf("%s", buffer);
	  fflush(stdout);

	  /*
	   * Le serveur renvoie "OK QUIT" s'il a re�u QUIT. On pr�f�rera 
	   * cela pour quitter proprement, plut�t que d'envoyer QUIT au
	   * serveur et de quitter c�t� client sans attendre de r�ponse. 
	   */
	  if (sscanf(buffer, "OK %s", buffer) == 1 && !strcmp(buffer, DETAIL_RET_QUIT))
	    break;
	}

    }
  }
  
  /* Fermeture de la connection */
  if (close(server_socket) == -1)
    {
      perror("Impossible de fermer la connection au serveur");
      return EXIT_FAILURE;
    }
  
  return EXIT_SUCCESS;
}
