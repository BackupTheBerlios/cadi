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

/* Retour de commande client */
#define RET_OK "OK"
#define RET_ERR "ERR"

/* Commandes spéciales */
#define CMD_CREATE_PROCESS "CreateProcess"
#define CMD_DESTROY_PROCESS "DestroyProcess"
#define CMD_QUIT "Quit"

/* Misc */
#define WELCOME_MSG "Welcome on CaDi daemon"

#define MAX_CHAR_MSG    1024 /* Taille maximale des messages */


#define INT_SIZE sizeof(INT_MAX)
#define forever for (;;)

/**
 * Affiche un message verbeux si le mode verbose est activé.
 */
static void VERBOSE(const char *format, ...) {
  va_list args;
  
  if (VERBOSE_FLAG) {
    va_start(args, format);
    vprintf(format, args);
    fflush(stdout);
  }
}

/**
 * Renvoie un pointeur sur une châine représentant un entier. Allouée dynamiquement.
 *
 * @param nb nombre dont récupérer la représentation en pointeur de char
 * @return la chaîne représentant nb
 */
static char *itoa(const int nb) {
  char *buf = malloc(INT_SIZE + 1);
  sprintf(buf, "%d", nb);
  return buf;
}

/**
 * Envoie une notification vers le socket précisé, précisant un succès ou une failure, avec des détails ou non.
 *
 * @param socket un socket
 * @param ok_or_fail un pointeur sur MSG_OK ou MSG_ERR typiquement
 * @param param un message de détail ou NULL
 */
static int send_notification(const int socket, const char *ok_or_fail, const char *param) {
  char msg[MAX_CHAR_MSG];

  strncpy(msg, ok_or_fail, MAX_CHAR_MSG - 1);
  if (param != NULL) {
    strcat(msg, " ");
    strcat(msg, param);
  }
  strcat(msg, "\n");

  if (send(socket, msg, strlen(msg) + 1, 0) < 0) {
    perror("send");
    return 0;
  }

  return 1;
}

/**
 * Envoie un message okas vers le socket précisé, avec des détails ou non.
 */
static int send_ok(const int socket, const char *param) {
  return send_notification(socket, RET_OK, param);
}

/**
 * Envoie un message de failure vers le socket précisé, avec des détails ou non.
 */
static int send_failure(const int socket, const char *param) {
  return send_notification(socket, RET_ERR, param);
}




static int server_received(const int client_socket, const char *msg) {
  char *s, token[MAX_CHAR_MSG]; /* Récupérera le 1er mot, ou la liste des paramètres, du message */
  pid_t new_proc;

  /* On saute les espaces */
  while (isspace(*msg)) msg++;
  
  /* Si on envoie rien.. on renvoie rien ! */
  if (*msg == '\0') { send_ok(client_socket, NULL); return MSG_OK; }
  
  /* Si on a pas du [0-9A-Za-z], on renvoie une erreur */
  if (!isalnum(*msg)) { send_failure(client_socket, NULL); return MSG_ERR; }
  
  /* On se place au début de la chaîne token */
  s = token;
  
  /* Tiens ! Mais on dirait un strcpy un peu modifié */
  while (!isspace(*msg))
    *s++ = *msg++;
  *s = '\0';
  
  /* On analyse le token */
  if (!strcmp(CMD_QUIT, token)) { send_ok(client_socket, NULL); return MSG_QUIT; }
  if (!strcmp(CMD_CREATE_PROCESS, token)) {
    while (isspace(*msg)) msg++;
    strcpy(token, msg);

    if ((new_proc = create_process(token))) { char *ps = itoa(new_proc); send_ok(client_socket, ps); free(ps); return MSG_OK; }
    return MSG_ERR;
  }

  return MSG_UNKNOWN_COMMAND;
}


void *treatClient(void *socketinfo) {
  char buffer[SERVER_BUFFER_SIZE];
  socketinfo_t *client_info;

  client_info = (socketinfo_t *) socketinfo;
  VERBOSE("Connection de %s ...\n", inet_ntoa(client_info->address.sin_addr));

  /* On envoie un message de bienvenue. Si ca merdoie, pas obligé de quitter, on continue. */
  if (send(client_info->socket, WELCOME_MSG "\n", sizeof(WELCOME_MSG) + 1, 0) < 0) { perror("send"); }
  
  /* On traite tous les messages du client */
  while (recv(client_info->socket, buffer, SERVER_BUFFER_SIZE, 0) > 0) {
    
    VERBOSE("Reçu : %s\n", buffer);

    /* On traite la commande et on vérifie s'il a demandé de quitter. ("quit") */
    if (!server_received(client_info->socket, buffer))
      break;
  }
  
  /* Déconnection du client */
  close(client_info->socket);

  VERBOSE("Déconnection de %s ...\n", inet_ntoa(client_info->address.sin_addr));
  
  pthread_exit(NULL);
}


int main() {
  int server_socket, client_socket, numcli;
  struct sockaddr_in server_address, client_address;
  socklen_t client_address_size;
  pthread_t threads[MAX_CLIENT];
  socketinfo_t client_info;

  /* Ouverture du socket */
  if ((server_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) { perror("socket"); return EXIT_FAILURE; }

  /* Initialisation du bind */
  memset(&server_address, 0, sizeof server_address);
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(SERVER_PORT);
	
  /* Ze bind */
  if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) { perror("bind"); return EXIT_FAILURE; }

  /* On le définit comme écouteur */
  if (listen(server_socket, MAX_CLIENT) < 0) { perror("listen"); return EXIT_FAILURE; }

  VERBOSE("Listening on port %d ...\n", SERVER_PORT);

  numcli = 0;
  client_address_size = sizeof client_address;

  /* Just for staille */
  forever {
  
    /* On attend une connection */
    if ((client_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_address_size)) < 0) {
      perror("accept");
      close(server_socket);
      return EXIT_FAILURE;
    }

    client_info.socket = client_socket;
    client_info.address = client_address;

    /* On crée le thread d'écoute */
    if (pthread_create(&threads[numcli], NULL, treatClient, &client_info)) { perror("pthread_create"); return EXIT_FAILURE; }

    numcli++;
  }

  close(server_socket);
	
  return EXIT_SUCCESS;
}
