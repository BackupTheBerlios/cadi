#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>

#include "cadid.h"
#include "process.h"

int snprintf(char *str, size_t size, const char *format, ...);
char * strdup (const char *s);

static const char *help = \
 DETAIL_RET_CREATE_PROCESS_SYNTAX " . . . Cr�er un processus\n" \
 DETAIL_RET_DESTROY_PROCESS_SYNTAX " . . . . . . . . . . D�truire un processus\n" \
 DETAIL_RET_SEND_INPUT_SYNTAX "  . . . . . . . . Envoyer des donn�es sur l'entr�e standard d'un processus\n" \
 DETAIL_RET_GET_OUTPUT_SYNTAX "  . . . . . . . . . . . . R�cup�rer la sortie standard d'un processus\n" \
 DETAIL_RET_GET_RETURN_CODE_SYNTAX "  . . . . . . . . . . R�cup�rer le code de retour d'un processus\n" \
 CMD_QUIT "  . . . . . . . . . . . . . . . . . Quitter\n" \
 CMD_GET_HELP "  . . . . . . . . . . . . . . . . . Afficher cette aide\n";

static unsigned port;

static int server_socket;
static int client_socket;
struct sockaddr_in server_address;
struct sockaddr_in client_address;

/**
 * Affiche l'aide de CaDiD.
 *
 * @param prog nom du programme (argv[0])
 */
static void usage(const char *prog)
{
    puts(SERVER_VERSION "\n");
    printf("Syntaxe : %s [ -v | -h | -p port ]\n", prog);
    puts("\t-p port . . . . . port local sur lequel se connecter");
    puts("\t-v  . . . . . . . afficher la version du serveur");
    puts("\t-h  . . . . . . . afficher cette aide");
}

/**
 * Affiche un message verbeux si le mode verbose est activ�.
 */
static void verbose(const char *format, ...)
{
    if (VERBOSE_FLAG) {
      va_list args;
      va_start(args, format);
      vfprintf(stdout, format, args);
      fflush(stdout);
    }
}

/**
 * Renvoie un pointeur sur une ch�ine statique repr�sentant un entier.
 *
 * @param nb nombre dont r�cup�rer la repr�sentation en pointeur de char
 * @return la cha�ne statique repr�sentant nb
 */
static char *itoa(const int nb)
{
    static char buf[8];
    sprintf(buf, "%d", nb);
    return buf;
}


static void client_open_connection() {
  verbose("Connection de %s ...\n", inet_ntoa(client_address.sin_addr));
}


static void client_close_connection() {
  verbose("D�connection du client %s ...\n", inet_ntoa(client_address.sin_addr));
  close(client_socket);
  client_socket = -1;
}

void send_basic(const int socket, const char *msg) {
  printf("to send basic : %s\n", msg);
  fflush(stdout);
  if (send(socket, msg, strlen(msg) + 1, 0) < 0)
    fprintf(stderr, "Impossible d'envoyer le message \"%s\" au client: %s", msg, strerror(errno));
}

/**
 * Envoie une notification vers le socket pr�cis�, pr�cisant un succ�s ou une failure, avec des d�tails ou non.
 *
 * @param socket un identifiant de socket
 * @param ok_or_fail MSG_OK ou MSG_ERR
 * @param param un message de d�tail ou NULL
 */
static void send_notification(const int socket, const char *ok_or_fail,
    const char *param)
{
    static char msg[MESSAGE_BUFFER_SIZE];

    snprintf(msg, MESSAGE_BUFFER_SIZE, "%s %s\n", ok_or_fail,
        (param != NULL ? param : ""));

    send_basic(socket, msg);
}

/**
 * Envoie un message okas vers le socket pr�cis�, avec des d�tails ou non.
 */
static void send_ok(const int socket, const char *param)
{
    send_notification(socket, RET_OK, param);
}

/**
 * Envoie un message de failure vers le socket pr�cis�, avec des d�tails ou non.
 */
static void send_failure(const int socket, const char *param)
{
    send_notification(socket, RET_ERR, param);
}

static int parse_client_line(const int client_socket, char *msg)
{
    char *token;

    /* On r�cup�re le premier mot, s'il est vide, on retourne direct */
    if ((token = strtok(msg, " ")) == NULL)
      return MSG_OK;


    /************/
    /* CMD_QUIT */
    /************/
    if (!strcmp(CMD_QUIT, token)) {
        send_ok(client_socket, DETAIL_RET_QUIT);
        return MSG_QUIT;
    }

    /**********************/
    /* CMD_CREATE_PROCESS */
    /**********************/
    else if (!strcmp(CMD_CREATE_PROCESS, token)) {
      pid_t proc;
      char *args[MAX_ARGS], **pc;

      pc = args;

      /* On r�cup le nom du prog */
      if ((token = strtok(NULL, " ")) == NULL) {
	send_failure(client_socket, DETAIL_RET_CREATE_PROCESS_SYNTAX);
	return MSG_ERR;
      }

      /* strtok renvoie un buffer static, on le copie */
      if ((*pc++ = strdup(token)) == NULL) { perror("strdup"); return MSG_ERR; }

      /* La suite devient optionelle, c'est les arguments */
      while ((token = strtok(NULL, " "))) {
	if ((*pc++ = strdup(token)) == NULL) { perror("strdup"); return MSG_ERR; }
      }

      *pc = NULL; /* Fin des arguments */

      /* On cr�e le processus */
      if ((proc = create_process(args[0], args))) {
	send_ok(client_socket, itoa(proc));
	return MSG_OK;
      }

      /* Le processus n'a pas pu �tre cr�e */
      send_failure(client_socket, DETAIL_RET_CREATE_PROCESS_ERROR);
      return MSG_ERR;
    }

    /***********************/
    /* CMD_DESTROY_PROCESS */
    /***********************/
    else if (!strcmp(CMD_DESTROY_PROCESS, token)) {
      pid_t process_to_kill;
      
      if ((token = strtok(NULL, " ")) == NULL) {
	send_failure(client_socket, DETAIL_RET_DESTROY_PROCESS_SYNTAX);
	return MSG_ERR;
      }
      
      process_to_kill = atoi(token);
	
      if (!process_exists(process_to_kill)) {
	send_failure(client_socket, DETAIL_RET_UNKNOWN_PROCESS);
	return MSG_ERR;
      }
      
      /* PAN */
      destroy_process(process_to_kill);
      send_ok(client_socket, NULL);
      return MSG_OK;
    }

    /******************/
    /* CMD_SEND_INPUT */
    /******************/
    else if (!strcmp(CMD_SEND_INPUT, token)) {
      pid_t send_to_process;
      char buffer[MESSAGE_BUFFER_SIZE];
      buffer[0] = '\0';

      /* On r�cup le PID */
      if ((token = strtok(NULL, " ")) == NULL) {
	send_failure(client_socket, DETAIL_RET_SEND_INPUT_SYNTAX);
	return MSG_ERR;
      }
      
      send_to_process = atoi(token);
      if (!process_exists(send_to_process)) {
	send_failure(client_socket, DETAIL_RET_UNKNOWN_PROCESS);
	return MSG_ERR;
      }
      
      /* On r�cup' le message � envoyer */
      while ((token = strtok(NULL, " "))) {
	strcat(buffer, token);
	strcat(buffer, " ");
      }

      /* Si le message est vide, erreur ! */
      if (strlen(buffer) == 0) {
	send_failure(client_socket, DETAIL_RET_SEND_INPUT_SYNTAX);
	return MSG_ERR;
      }

      printf("� envoyer : %s\n", buffer);
      
      /* Sinon on envoie ! */
      send_input(send_to_process, buffer);
      send_ok(client_socket, NULL);
      return MSG_OK;
    }

    /******************/
    /* CMD_GET_OUTPUT */
    /******************/
    else if (!strcmp(CMD_GET_OUTPUT, token)) {
      pid_t process_to_get_output;

      if ((token = strtok(NULL, " ")) == NULL) {
	send_failure(client_socket, DETAIL_RET_GET_OUTPUT_SYNTAX);
	return MSG_ERR;
      }

      process_to_get_output = atoi(token);
	
      if (!process_exists(process_to_get_output)) {
	send_failure(client_socket, DETAIL_RET_UNKNOWN_PROCESS);
	return MSG_ERR;
      }
      
      /* PAN */
      get_output(client_socket, process_to_get_output);
      send_ok(client_socket, NULL);
      return MSG_OK;
    }

    /***********************/
    /* CMD_GET_RETURN_CODE */
    /***********************/
    else if (!strcmp(CMD_GET_RETURN_CODE, token)) {
      pid_t process_to_get_ret;
      int ret;

      if ((token = strtok(NULL, " ")) == NULL) {
	send_failure(client_socket, DETAIL_RET_GET_RETURN_CODE_SYNTAX);
	return MSG_ERR;
      }

      process_to_get_ret = atoi(token);
	
      if (!process_exists(process_to_get_ret)) {
	send_failure(client_socket, DETAIL_RET_UNKNOWN_PROCESS);
	return MSG_ERR;
      }
      
      /* PAN */
      ret = get_return_code(process_to_get_ret);
      if (ret == PROCESS_NOT_TERMINATED) {
	send_failure(client_socket, DETAIL_RET_GET_RETURN_CODE_ERROR);
	return MSG_ERR;
      }
      
      send_ok(client_socket, itoa(ret));
      return MSG_OK;
    }

    /****************/
    /* CMD_GET_HELP */
    /****************/
    else if (!strcmp(CMD_GET_HELP, token)) {
      send_basic(client_socket, help);
      return MSG_OK;
    }

    /**************************/
    /* Commande inconnue .. ! */
    /**************************/

    send_failure(client_socket, DETAIL_RET_UNKNOWN_COMMAND);
    return MSG_UNKNOWN_COMMAND;
}

/**
 * Traite la connection avec le client.
 */
void process_client()
{
  char buffer[MESSAGE_BUFFER_SIZE];
  
  /* Le client vient de se connecter */
  client_open_connection();

  /* On envoie un message de bienvenue et le prompt */
  send_basic(client_socket, WELCOME_MSG "\n");
  send_basic(client_socket, CLIENT_PROMPT);
  
  /* On traite tous les messages du client */
  while (recv(client_socket, buffer, MESSAGE_BUFFER_SIZE, 0) > 0) {
    verbose("Client # %s\n", buffer);
    
    /* On traite la commande */
    if (!parse_client_line(client_socket, buffer))
      break;
    
    send_basic(client_socket, CLIENT_PROMPT);
  }
  
  /* Si on arrive ici, c'est que le client a quitt�. */
  client_close_connection();
}

/**
 * Parse la ligne de commande.
 */
static void parse_command_line(int argc, char *argv[])
{
  char *prog = argv[0];
  argc = argc; /* Evite un warning */
  port = DEFAULT_PORT;

  while (*++argv) {
    /* Version */
    if (!strcmp(*argv, "-v")) { puts(SERVER_VERSION); exit(EXIT_SUCCESS); }

    /* Aide */
    else if (!strcmp(*argv, "-h")) { usage(prog); exit(EXIT_SUCCESS); }
    
    /* Port */
    else if (!strcmp(*argv, "-p")) {
      if (*(argv + 1) == NULL) { usage(prog); exit(EXIT_FAILURE); }
      port = atoi(*++argv);
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
 * Fonction de callback appel�e lors d'un ctrl+c. Elle ferme les connections et quitte proprement.
 */
static void trap_ctrlc(int sig) {
  sig = sig; /* Evite un warning */

  puts("Fermeture du d�mon ...");
  if (client_socket > 0)
    client_close_connection(client_socket);
  close(server_socket);

  exit(EXIT_SUCCESS);
}

/**
 * Fonction principale.
 */
int main(int argc, char *argv[])
{
  socklen_t client_address_size;

    /* V�rifie la ligne de commande */
    parse_command_line(argc, argv);

    /* On cr�e un socket pour se connecter sur un serveur */
    if ((server_socket = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Impossible d'ouvrir de socket");
        return EXIT_FAILURE;
    }

    /* Initialisation du bind */
    memset(&server_address, 0, sizeof server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    /* Ze bind */
    if (bind(server_socket, (struct sockaddr *) &server_address,
            sizeof(server_address)) == -1) {
        perror("Impossible de se binder");
	if (close(server_socket) == -1)
	    perror("Impossible de fermer le socket serveur");
        return EXIT_FAILURE;
    }

    /* On le d�finit comme �couteur */
    if (listen(server_socket, MAX_CLIENT) == -1) {
        perror("Impossible de mettre le socket serveur en �coute");
	if (close(server_socket) == -1)
	  perror("Impossible de fermer le socket serveur");
        return EXIT_FAILURE;
    }
    
    verbose("D�marrage du d�mon sur le port %d ...\n", port);

    /* Pour quitter le serveur proprement */
    signal(SIGINT, trap_ctrlc);
    
    /* Variable concr�te car il faut pouvoir en avoir l'adresse pour accept() */
    client_address_size = sizeof client_address;

    for (;;) {
      /* On attend une connection */
      if ((client_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_address_size)) == -1) {
	perror("Impossible d'accepter la connection d'un client");
	if (close(server_socket) == -1)
	  perror("Impossible de fermer le socket serveur");
	return EXIT_FAILURE;
      }
      
      /* On traite ce client */
      process_client();
    }
    
    if (close(server_socket) == -1) {
      perror("Impossible de fermer le socket serveur");
      return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
