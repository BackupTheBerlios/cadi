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
#include <pthread.h>
#include <limits.h>

#include "cadid.h"
#include "process.h"

#define CADI_SERVER_VERSION "CaDi Server v0.1a\n"

static const char *help = "\n" \
 DETAIL_RET_CREATE_PROCESS_SYNTAX " . . . Créer un processus\n" \
 DETAIL_RET_DESTROY_PROCESS_SYNTAX " . . . . . . . . . . Détruire un processus\n" \
 DETAIL_RET_SEND_INPUT_SYNTAX "  . . . . . . . . Envoyer des données sur l'entrée standard d'un processus\n" \
 DETAIL_RET_GET_OUTPUT_SYNTAX "  . . . . . . . . . . . . Récupérer la sortie standard d'un processus\n" \
 DETAIL_RET_GET_RETURN_CODE_SYNTAX "  . . . . . . . . . . Récupérer le code de retour d'un processus\n" \
 CMD_QUIT "  . . . . . . . . . . . . . . . . . Quitter\n" \
 CMD_GET_HELP "  . . . . . . . . . . . . . . . . . Afficher cette aide";

static unsigned port;

/**
 * Affiche l'aide de CaDiD.
 *
 * @param prog nom du programme (argv[0])
 */
static void usage(const char *prog)
{
    puts(CADI_SERVER_VERSION);
    printf("Syntaxe : %s [-v | port]\n", prog);
    puts("-v\tVersion du serveur");
    fflush(stdout);
}

/**
 * Affiche un message verbeux si le mode verbose est activé.
 */
static void VERBOSE(const char *format, ...)
{
    va_list args;

    if (VERBOSE_FLAG) {
        va_start(args, format);
        vprintf(format, args);
        fflush(stdout);
    }
}

/**
 * Renvoie un pointeur sur une châine représentant un entier. Allouée statiquement.
 *
 * @param nb nombre dont récupérer la représentation en pointeur de char
 * @return la chaîne représentant nb
 */
static char *itoa(const int nb)
{
    static char buf[sizeof(INT_MAX) + 1];
    sprintf(buf, "%d", nb);
    return buf;
}

/**
 * Envoie une notification vers le socket précisé, précisant un succès ou une failure, avec des détails ou non.
 *
 * @param socket un identifiant de socket
 * @param ok_or_fail MSG_OK ou MSG_ERR
 * @param param un message de détail ou NULL
 */
static void send_notification(const int socket, const char *ok_or_fail,
    const char *param)
{
    static char msg[MESSAGE_BUFFER_SIZE];

    int snprintf(char *str, size_t size, const char *format, ...);
    snprintf(msg, MESSAGE_BUFFER_SIZE, "%s %s", ok_or_fail,
        (param != NULL ? param : ""));

    if (send(socket, msg, strlen(msg) + 1, 0) < 0)
        perror("Impossible d'envoyer de message au client");

    /* On arrête pas le serveur pour autant */
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


static int server_received(const int client_socket, const char *msg)
{
    char *s, token[MESSAGE_BUFFER_SIZE];    /* Récupérera le 1er mot, ou la liste des paramètres, du message */
    pid_t new_proc;

    /* Si on a pas du [0-9A-Za-z], on renvoie une erreur */
    if (!isalnum(*msg)) {
        send_failure(client_socket, NULL);
        return MSG_ERR;
    }

    /* On se place au début de la chaîne token */
    s = token;

    /* On copie le premier mot. BUG si on dépasse MESSAGE_BUFFER_SIZE hihi
       Mais le client envoie au maximum ça, donc NP. */
    while (!isspace(*msg))
        *s++ = *msg++;
    *s = '\0';

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
      /* On saute les espaces entre la commande et les params */
      while (isspace(*msg))
	msg++;

      if (strlen(msg) == 0) {
	send_failure(client_socket, DETAIL_RET_CREATE_PROCESS_SYNTAX);
	return MSG_ERR;
      }

      /* On copie le reste de la chaîne dans token (la commande a exécuter) */
      strcpy(token, msg);

      if ((new_proc = create_process(token))) {
	char *ps = itoa(new_proc);
	send_ok(client_socket, ps);
	return MSG_OK;
      }

      /* Le processus n'a pas pu être crée */
      send_failure(client_socket, DETAIL_RET_CREATE_PROCESS_ERROR);
      return MSG_ERR;
    }

    /***********************/
    /* CMD_DESTROY_PROCESS */
    /***********************/
    else if (!strcmp(CMD_DESTROY_PROCESS, token)) {
      pid_t process_to_kill;

      /* On saute les espaces */
      while (isspace(*msg))
	msg++;
      
      if (strlen(msg) == 0) {
	send_failure(client_socket, DETAIL_RET_DESTROY_PROCESS_SYNTAX);
	return MSG_ERR;
      }
      
      /* On copie le paramètre correspondant au pid */
      s = token;
      while (*msg != '\0') {
	if (!isdigit(*msg)) {
	  send_failure(client_socket, DETAIL_RET_DESTROY_PROCESS_SYNTAX);
	  return MSG_ERR;
	}
	*s++ = *msg++;
      }
      *s = '\0';
      
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

	/* On saute les espaces */
        while (isspace(*msg))
            msg++;

	if (strlen(msg) == 0) {
	  send_failure(client_socket, DETAIL_RET_SEND_INPUT_SYNTAX);
	  return MSG_ERR;
	}

	/* On copie le paramètre correspondant au pid */
	s = token;
	while (!isspace(*msg)) {
	  if (!isdigit(*msg)) {
	    send_failure(client_socket, DETAIL_RET_SEND_INPUT_SYNTAX);
	    return MSG_ERR;
	  }
	  *s++ = *msg++;
	}
	*s = '\0';

	send_to_process = atoi(token);
	if (!process_exists(send_to_process)) {
	  send_failure(client_socket, DETAIL_RET_UNKNOWN_PROCESS);
	  return MSG_ERR;
	}

	/* On saute les espaces */
        while (isspace(*msg))
            msg++;

	if (strlen(msg) == 0) {
	  send_failure(client_socket, DETAIL_RET_SEND_INPUT_SYNTAX);
	  return MSG_ERR;
	}

	send_input(send_to_process, msg);
	send_ok(client_socket, NULL);
	return MSG_OK;
    }

    /******************/
    /* CMD_GET_OUTPUT */
    /******************/
    else if (!strcmp(CMD_GET_OUTPUT, token)) {
      pid_t process_to_get_output;
      char *output;

      /* On saute les espaces */
      while (isspace(*msg))
	msg++;
      
      if (strlen(msg) == 0) {
	send_failure(client_socket, DETAIL_RET_GET_OUTPUT_SYNTAX);
	return MSG_ERR;
      }
      
      /* On copie le paramètre correspondant au pid */
      s = token;
      while (*msg != '\0') {
	if (!isdigit(*msg)) {
	  send_failure(client_socket, DETAIL_RET_GET_OUTPUT_SYNTAX);
	  return MSG_ERR;
	}
	*s++ = *msg++;
      }
      *s = '\0';
      
      process_to_get_output = atoi(token);
	
      if (!process_exists(process_to_get_output)) {
	send_failure(client_socket, DETAIL_RET_UNKNOWN_PROCESS);
	return MSG_ERR;
      }
      
      /* PAN */
      output = get_output(process_to_get_output);
      send_ok(client_socket, output);
      return MSG_OK;
    }

    /***********************/
    /* CMD_GET_RETURN_CODE */
    /***********************/
    else if (!strcmp(CMD_GET_RETURN_CODE, token)) {
      pid_t process_to_get_rc;
      int rc;

      /* On saute les espaces */
      while (isspace(*msg))
	msg++;
      
      if (strlen(msg) == 0) {
	send_failure(client_socket, DETAIL_RET_GET_RETURN_CODE_SYNTAX);
	return MSG_ERR;
      }
      
      /* On copie le paramètre correspondant au pid */
      s = token;
      while (*msg != '\0') {
	if (!isdigit(*msg)) {
	  send_failure(client_socket, DETAIL_RET_GET_RETURN_CODE_SYNTAX);
	  return MSG_ERR;
	}
	*s++ = *msg++;
      }
      *s = '\0';
      
      process_to_get_rc = atoi(token);
	
      if (!process_exists(process_to_get_rc)) {
	send_failure(client_socket, DETAIL_RET_UNKNOWN_PROCESS);
	return MSG_ERR;
      }
      
      /* PAN */
      rc = get_return_code(process_to_get_rc);
      if (rc == PROCESS_NOT_TERMINATED) {
	send_failure(client_socket, DETAIL_RET_GET_RETURN_CODE_ERROR);
	return MSG_ERR;
      }
      
      send_ok(client_socket, itoa(rc));
      return MSG_OK;
    }

    /****************/
    /* CMD_GET_HELP */
    /****************/
    else if (!strcmp(CMD_GET_HELP, token)) {
      send_ok(client_socket, help);
      return MSG_OK;
    }

    return MSG_UNKNOWN_COMMAND;
}

/**
 * Traite la connection avec le client dont l'id socket est client_socket.
 *
 * @param client_socket l'id socket du client
 * @param client_address les informations sur la connection du client
 */
void process_client(int client_socket, struct sockaddr_in client_address)
{
    char buffer[MESSAGE_BUFFER_SIZE];
    int ret;

    /* Blabla */
    VERBOSE("Connection de %s ...\n", inet_ntoa(client_address.sin_addr));

    /* On envoie un message de bienvenue. Si ca merdoie, pas obligé de quitter, on continue. */
    if (send(client_socket, WELCOME_MSG, sizeof(WELCOME_MSG), 0) < 0)
        perror("Impossible d'envoyer le message de bienvenue");

    /* On traite tous les messages du client */
    while (recv(client_socket, buffer, MESSAGE_BUFFER_SIZE, 0) > 0) {
        VERBOSE("Reçu : %s\n", buffer);

        /* On traite la commande et on vérifie s'il a demandé de quitter. */
        ret = server_received(client_socket, buffer);
	if (ret == MSG_QUIT)
            break;
	else if (ret == MSG_UNKNOWN_COMMAND) {
	  send_failure(client_socket, DETAIL_RET_UNKNOWN_COMMAND);
	}
    }

    /* Si on arrive ici, c'est que le client a quitté. */
    close(client_socket);
    VERBOSE("Déconnection de %s ...\n", inet_ntoa(client_address.sin_addr));
}

/**
 * Parse la ligne de commande.
 */
static void parse_command_line(int argc, char *argv[])
{

    /* Cas foireux */
    if (argc > 2) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Config par défaut */
    if (argc == 1) {
        port = SERVER_PORT;
        return;
    }

    /* Au moins la version spécifiée */
    if (argc > 1) {

        /* Version */
        if (!strcmp(argv[1], "-v")) {
            printf(CADI_SERVER_VERSION);
            exit(EXIT_SUCCESS);
        }

        if (!strcmp(argv[1], "-h")) {
            usage(argv[0]);
            exit(EXIT_SUCCESS);
        }

        /* Un port ? */
        if (!isdigit(*argv[1])) {   /* La vérif' est faible, tant pis */
            fprintf(stderr, "Le port doit être numérique.\n");
            exit(EXIT_FAILURE);
        }

        port = atoi(argv[1]);
    }
}

int main(int argc, char *argv[])
{
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_size;

    /* Vérifie la ligne de commande */
    parse_command_line(argc, argv);

    /* On crée un socket pour se connecter sur un serveur */
    if ((server_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
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
            sizeof(server_address)) < 0) {
        perror("Impossible de se binder");
        return EXIT_FAILURE;
    }

    /* On le définit comme écouteur */
    if (listen(server_socket, MAX_CLIENT) < 0) {
        perror("Impossible de se mettre en écoute");
        return EXIT_FAILURE;
    }

    VERBOSE("Écoute du port %d ...\n", port);
    client_address_size = sizeof client_address;

    for (;;) {
        /* On attend une connection */
        if ((client_socket =
                accept(server_socket, (struct sockaddr *) &client_address,
                    &client_address_size)) < 0) {
            perror("Impossible d'accepter la connection d'un client");
            close(server_socket);
            return EXIT_FAILURE;
        }

        /* On traite ce client */
        process_client(client_socket, client_address);
    }

    close(server_socket);

    return EXIT_SUCCESS;
}
