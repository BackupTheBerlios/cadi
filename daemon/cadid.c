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

#include "config.h"
#include "process.h"
#include "cadid.h"

/*
 * Contient les informations sur un socket (son id et sa structure de
 * connection) 
 */
typedef struct {
  int socket;
  struct sockaddr_in address;
} socketinfo_t;

extern char *strdup(const char *);
extern int gethostname(char *, size_t);

static const char *server_version = "CaDiD v0.1a";
static const char *welcome = "Welcome on a cadid's server";

static const char *prompt_client = "$ ";
static const char *prompt_server = "! ";

static const int HOST_SIZE = 100;
static const int MAX_CLIENT = 1;

static unsigned port;
static bool verbose_flag;

static int server_socket;
static int client_socket;
struct sockaddr_in server_address;
struct sockaddr_in client_address;

static const char *help =
 DETAIL_RET_CREATE_PROCESS_SYNTAX                 " . . . Créer un processus\n"
 DETAIL_RET_DESTROY_PROCESS_SYNTAX  " . . . . . . . . . . Détruire un processus\n"
 DETAIL_RET_SEND_INPUT_SYNTAX          ". . . . . . . . . Envoyer des données sur l'entrée standard d'un processus\n"
 DETAIL_RET_CLOSE_INPUT_SYNTAX  " . . . . . . . . . . . . Fermer le flux d'entrée standard d'un processus\n"
 DETAIL_RET_GET_OUTPUT_SYNTAX  ". . . . . . . . . . . . . Récupérer la sortie standard d'un processus\n"
 DETAIL_RET_GET_ERROR_SYNTAX  " . . . . . . . . . . . . . Récupérer la sortie d'erreur d'un processus\n"
 DETAIL_RET_GET_RETURN_CODE_SYNTAX ". . . . . . . . . . . Récupérer le code de retour d'un processus\n"
 DETAIL_RET_LIST_PROCESS_SYNTAX " . . . . . . . . . . . . . . Lister les processus exécutés\n"
 CMD_QUIT            ". . . . . . . . . . . . . . . . . . Quitter\n"
 CMD_GET_HELP        ". . . . . . . . . . . . . . . . . . Afficher cette aide\n";

/**
 * Affiche l'aide du serveur.
 *
 * @param prog nom du programme (argv[0])
 */
static void usage(const char *prog)
{
  puts(server_version);
  printf("Usage : %s [ -v | -V | -h | -p port ]\n", prog);
  printf("\t-p port . . . . . port local sur lequel se connecter (défault %d)\n", DEFAULT_PORT);
  puts("\t-v  . . . . . . . afficher la version du serveur");
  puts("\t-V  . . . . . . . mode verbose");
  puts("\t-h  . . . . . . . afficher cette aide");
}

/**
 * Affiche un message verbeux si le mode verbose est activé.
 */
static void verbose(const char *format, ...)
{
  if (verbose_flag)
    {
      va_list args;
      va_start(args, format);
      vprintf(format, args);
      fflush(stdout);
    }
}

/**
 * Renvoie un pointeur sur une chaîne statique représentant un entier.
 *
 * @param nb nombre dont récupérer la représentation en pointeur de char
 * @return la chaîne statique représentant nb
 */
static char *itoa(const int nb)
{
  static char buf[10];
  snprintf(buf, sizeof buf, "%d", nb);
  return buf;
}


static void client_open_connection()
{
  verbose("Connection de %s ...\n", inet_ntoa(client_address.sin_addr));
}


static void client_close_connection()
{
  verbose("Déconnection de %s ...\n", inet_ntoa(client_address.sin_addr));
  if (close(client_socket) == -1)
    perror("Impossible de fermer le socket client");
  client_socket = -1;
}

void send_basic(const int socket, const void *msg, unsigned sz)
{
  if (send(socket, msg, sz, 0) == -1)
    perror("send");
}

/**
 * Envoie une notification vers le socket précisé, précisant un succès ou une failure, avec des détails ou non.
 *
 * @param socket un identifiant de socket
 * @param ok_or_fail MSG_OK ou MSG_ERR
 * @param param un message de détail ou NULL
 */
static void send_notification(const int socket, const char *ok_or_fail, const char *param)
{
  static char msg[MESSAGE_BUFFER_SIZE];
  snprintf(msg, MESSAGE_BUFFER_SIZE, "%s %s\n", ok_or_fail, (param != NULL ? param : ""));
  send_basic(socket, msg, strlen(msg));
}

/**
 * Envoie un message okas vers le socket précisé, avec des détails ou non.
 */
static void send_ok(const int socket, const char *param)
{
  send_notification(socket, RET_OK, param);
}

/**
 * Envoie un message de failure vers le socket précisé, avec des détails ou non.
 */
static void send_failure(const int socket, const char *param)
{
  send_notification(socket, RET_ERR, param);
}

static int parse_client_line(const int client_socket, char *msg)
{
  char *token;
  
  /* On récupère le premier mot, s'il est vide, on retourne direct  */
  if (!(token = strtok(msg, " ")))
    return MSG_OK;

  /*****************************************************************************
   *                              CMD_QUIT
   ****************************************************************************/
  if (!strcmp(CMD_QUIT, token))
    {
      send_ok(client_socket, DETAIL_RET_QUIT);
      return MSG_QUIT;
    }
  
  /*****************************************************************************  
   *                          CMD_CREATE_PROCESS 
   ****************************************************************************/
  else if (!strcmp(CMD_CREATE_PROCESS, token))
    {
      char *args[MAX_ARGS];
      char **pc = args;

      /* On récup le nom du prog */
      if (!(token = strtok(NULL, " ")))
	{
	  send_failure(client_socket, DETAIL_RET_CREATE_PROCESS_SYNTAX);
	  return MSG_ERR;
	}
      
      /* strtok renvoie un buffer static, on le copie */
      /* *pc = args[0] = nom du programme */
      if (!(*pc++ = strdup(token))) 
	{
	  perror("strdup");
	  return MSG_ERR;
	}
      
      /* La suite devient optionelle, c'est les arguments */
      while ((token = strtok(NULL, " ")))
	{
	  if ((*pc++ = strdup(token)) == NULL)
	    {
	      perror("strdup");
	      return MSG_ERR;
	    }
	}
      
      *pc = NULL;             /* Fin des arguments */
      
      /* On crée le processus */
      pid_t proc = create_process(args[0], args);

      /* Le processus n'a pas pu être créé */
      if (proc == -1) {
	send_failure(client_socket, DETAIL_RET_CREATE_PROCESS_ERROR);
	return MSG_ERR;
      }

      send_ok(client_socket, itoa(proc));
      return MSG_OK;
    }

  /*****************************************************************************  
   *                          CMD_DESTROY_PROCESS 
   ****************************************************************************/
  else if (!strcmp(CMD_DESTROY_PROCESS, token))
    {
      if ((token = strtok(NULL, " ")) == NULL)
	{
	  send_failure(client_socket, DETAIL_RET_DESTROY_PROCESS_SYNTAX);
	  return MSG_ERR;
	}
      
      pid_t process_to_kill = atoi(token);
      
      if (!process_exists(process_to_kill))
	{
	  send_failure(client_socket, DETAIL_RET_UNKNOWN_PROCESS);
	  return MSG_ERR;
	}
      
      destroy_process(process_to_kill);
      send_ok(client_socket, NULL);
      return MSG_OK;
    }

  /*****************************************************************************  
   *                          CMD_SEND_INPUT      
   ****************************************************************************/
  else if (!strcmp(CMD_SEND_INPUT, token))
    {
      char buffer[MESSAGE_BUFFER_SIZE];
      buffer[0] = '\0';
      
      /* On récup le PID */
      if ((token = strtok(NULL, " ")) == NULL)
	{
	  send_failure(client_socket, DETAIL_RET_SEND_INPUT_SYNTAX);
	  return MSG_ERR;
	}
      
      /* Il existe ? */
      pid_t send_to_process = atoi(token);
      if (!process_exists(send_to_process))
	{
	  send_failure(client_socket, DETAIL_RET_UNKNOWN_PROCESS);
	  return MSG_ERR;
	}
      
      /* Il est déjà terminé ? */
      if (get_return_code(send_to_process) != PROCESS_NOT_TERMINATED)
	{
	  send_failure(client_socket, DETAIL_RET_PROCESS_TERMINATED);
	  return MSG_ERR;
	}

      /* Son stdin est ouvert ? */
      if (!input_open(send_to_process))
	{
	  send_failure(client_socket, DETAIL_RET_INPUT_CLOSE);
	  return MSG_ERR;
	}

      /* On récup' le message à envoyer  */
      /* TODO: Prendre la chaîne telle qu'elle, sans splitter puis merger avec un espace */
      while ((token = strtok(NULL, " ")))
	{
	  strcat(buffer, token);
	  strcat(buffer, " ");
	}
      
      /* Si le message est vide, erreur ! */
      if (strlen(buffer) == 0)
	{
	  send_failure(client_socket, DETAIL_RET_SEND_INPUT_SYNTAX);
	  return MSG_ERR;
        }
      
      /* Sinon on envoie ! */
      send_input(send_to_process, buffer);
      send_ok(client_socket, NULL);
      return MSG_OK;
    }


  /*****************************************************************************  
   *                          CMD_CLOSE_INPUT     
   ****************************************************************************/
  else if (!strcmp(CMD_CLOSE_INPUT, token))
    {
      if ((token = strtok(NULL, " ")) == NULL)
	{
	  send_failure(client_socket, DETAIL_RET_CLOSE_INPUT_SYNTAX);
	  return MSG_ERR;
        }
      
      pid_t process_to_close_input = atoi(token);
      if (!process_exists(process_to_close_input))
	{
	  send_failure(client_socket, DETAIL_RET_UNKNOWN_PROCESS);
	  return MSG_ERR;
	}

      close_input(process_to_close_input);
      send_ok(client_socket, NULL);
      return MSG_OK;
    }
  
  /*****************************************************************************  
   *                          CMD_GET_OUTPUT
   ****************************************************************************/
  else if (!strcmp(CMD_GET_OUTPUT, token))
    {
      if ((token = strtok(NULL, " ")) == NULL)
	{
	  send_failure(client_socket, DETAIL_RET_GET_OUTPUT_SYNTAX);
	  return MSG_ERR;
	}
      
      pid_t process_to_get_output = atoi(token);
      if (!process_exists(process_to_get_output))
	{
	  send_failure(client_socket, DETAIL_RET_UNKNOWN_PROCESS);
	  return MSG_ERR;
        }
     
      get_output(client_socket, process_to_get_output);
      send_ok(client_socket, NULL);
      return MSG_OK;
    }


  /*****************************************************************************  
   *                          CMD_GET_ERROR
   ****************************************************************************/
  else if (!strcmp(CMD_GET_ERROR, token))
    {
      if ((token = strtok(NULL, " ")) == NULL)
	{
	  send_failure(client_socket, DETAIL_RET_GET_ERROR_SYNTAX);
	  return MSG_ERR;
        }
      
      pid_t process_to_get_error = atoi(token);
      if (!process_exists(process_to_get_error))
	{
	  send_failure(client_socket, DETAIL_RET_UNKNOWN_PROCESS);
	  return MSG_ERR;
	}
      
      get_error(client_socket, process_to_get_error);
      send_ok(client_socket, NULL);
      return MSG_OK;
    }


  /*****************************************************************************  
   *                          CMD_GET_RETURN_CODE
   ****************************************************************************/
  else if (!strcmp(CMD_GET_RETURN_CODE, token))
    {
      if ((token = strtok(NULL, " ")) == NULL)
	{
	  send_failure(client_socket, DETAIL_RET_GET_RETURN_CODE_SYNTAX);
	  return MSG_ERR;
        }
      
      pid_t process_to_get_ret = atoi(token);
      if (!process_exists(process_to_get_ret))
	{
	  send_failure(client_socket, DETAIL_RET_UNKNOWN_PROCESS);
	  return MSG_ERR;
	}
      
      int ret = get_return_code(process_to_get_ret);
      if (ret == PROCESS_NOT_TERMINATED)
	{
	  send_failure(client_socket, DETAIL_RET_GET_RETURN_CODE_ERROR);
	  return MSG_ERR;
	}
      
      send_ok(client_socket, itoa(ret));
      return MSG_OK;
    }

  /*****************************************************************************  
   *                          CMD_LIST_PROCESS   
   ****************************************************************************/
  else if (!strcmp(CMD_LIST_PROCESS, token))
    {
      list_process(client_socket);
      send_ok(client_socket, NULL);
      return MSG_OK;
    }

  /*****************************************************************************  
   *                          CMD_GET_HELP
   ****************************************************************************/
  else if (!strcmp(CMD_GET_HELP, token))
    {
      send_basic(client_socket, help, strlen(help));
      return MSG_OK;
    }

  /*****************************************************************************  
   *                        COMMANDE INCONNUE
   ****************************************************************************/
  else
    {
      send_failure(client_socket, DETAIL_RET_UNKNOWN_COMMAND);
      return MSG_UNKNOWN_COMMAND;
    }
}

/**
 * Traite la connection avec le client.
 */
void process_client()
{
  char buffer[MESSAGE_BUFFER_SIZE];
  char host[HOST_SIZE];
  
  /* Le client vient de se connecter */
  client_open_connection();
  
  if (gethostname(host, sizeof host) == -1 && errno == EINVAL)
    host[sizeof host - 1] = '\0';
  
  /* On envoie un message de bienvenue et le prompt */
  snprintf(buffer, sizeof buffer, "%s [ %s ]\n", welcome, host);
  send_basic(client_socket, buffer, strlen(buffer));
  send_basic(client_socket, prompt_client, strlen(prompt_client));
  
  /* On traite tous les messages du client */
  while (recv(client_socket, buffer, MESSAGE_BUFFER_SIZE, 0) > 0)
    {
      verbose("Client # %s\n", buffer);
      
      /* On traite la commande  */
      if (parse_client_line(client_socket, buffer) == MSG_QUIT)
	break;
      
      send_basic(client_socket, prompt_client, strlen(prompt_client));
    }
  
  /* Si on arrive ici, c'est que le client a quitté. */
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
    if (!strcmp(*argv, "-v"))
      {
	puts(server_version);
	exit(EXIT_SUCCESS);
      }
    
    /* Verbose */
    if (!strcmp(*argv, "-V"))
      {
	verbose_flag = true;
      }

    /* Aide  */
    else if (!strcmp(*argv, "-h"))
      {
	usage(prog);
	exit(EXIT_SUCCESS);
      }
    
    /* Port  */
    else if (!strcmp(*argv, "-p"))
      {
	if (*(argv + 1) == NULL)
	  {
	    usage(prog);
	    exit(EXIT_FAILURE);
	  }
	port = atoi(*++argv);
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
 * Arrête le serveur proprement en déconnectant les clients et lui même.
 */
static void shutdown_server() {
  puts("Fermeture du démon ...");

  /* Fermeture du client */
  if (client_socket > 0)
    client_close_connection(client_socket);

  /* Fermeture du serveur */
  if (close(server_socket) == -1)
    {
      perror("Impossible de fermer le socket serveur");
      exit(EXIT_FAILURE);
    }
  
  exit(EXIT_SUCCESS);
}


/**
 * Fonction de callback appelée lors d'un ctrl+c.
 */
static void trap_ctrlc(int sig)
{
  sig = sig;                  /* Evite un warning */
  shutdown_server();
}

/**
 * Point d'entrée du programme.
 */
int main(int argc, char *argv[])
{
  parse_command_line(argc, argv);
  
  /* On crée un socket pour se connecter sur un serveur */
  if ((server_socket = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror("socket");
      return EXIT_FAILURE;
    }
  
  /* Initialisation du bind */
  memset(&server_address, 0, sizeof server_address);
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(port);
  
  /* Ze bind  */
  if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) == -1)
    {
      perror("bind");
      if (close(server_socket) == -1)
	perror("Impossible de fermer le socket serveur");
      return EXIT_FAILURE;
    }
  
  /* On le définit comme écouteur */
  if (listen(server_socket, MAX_CLIENT) == -1)
    {
      perror("Impossible de mettre le socket serveur en écoute");
      if (close(server_socket) == -1)
	perror("Impossible de fermer le socket serveur");
      return EXIT_FAILURE;
    }
  
  verbose("Démarrage du démon sur le port %d ...\n", port);
  
  /* Pour quitter le serveur proprement  */
  signal(SIGINT, trap_ctrlc);
  
  for (;;) {
    
    /* Variable concrète car il faut pouvoir en avoir l'adresse pour accept() */
    socklen_t client_address_size = sizeof client_address;
    
    /* On attend une connection */
    if ((client_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_address_size)) == -1)
      {
	perror("accept");
	if (close(server_socket) == -1)
	  perror("Impossible de fermer le socket serveur");
	return EXIT_FAILURE;
      }
    
    /* On traite ce client */
    process_client();
  }
  
  shutdown_server(); /* N'arrivera jamais */
  
  return EXIT_FAILURE;
}
