#ifndef CADID_H
#define CADID_H

#define MESSAGE_BUFFER_SIZE 1024    /* Taille maximale des messages transmis */
#define MAX_ERR_SIZE        255 /* Taille maximale du message d'erreur */

#define SERVER_PORT         53553   /* Le port de CaDiD */
#define MAX_CLIENT          1   /* Un seul client peut se connecter (pour l'instant) */

/* Type de la commande reçue */
#define MSG_QUIT            0
#define MSG_ERR             1
#define MSG_OK              2
#define MSG_UNKNOWN_COMMAND 3

/* Commandes spéciales */
#define CMD_CREATE_PROCESS  "CreateProcess"
#define CMD_DESTROY_PROCESS "DestroyProcess"
#define CMD_SEND_INPUT      "SendInput"
#define CMD_GET_OUTPUT      "GetOutput"
#define CMD_GET_RETURN_CODE "GetReturnCode"
#define CMD_QUIT            "Quit"
#define CMD_GET_HELP        "Help"

/* Retour au client de sa commande */
#define RET_OK  "OK"
#define RET_ERR "ERR"

/* Détail de réponse */
#define DETAIL_RET_QUIT "QUIT"
#define DETAIL_RET_DESTROY_PROCESS_SYNTAX CMD_DESTROY_PROCESS " <id>"
#define DETAIL_RET_CREATE_PROCESS_SYNTAX  CMD_CREATE_PROCESS " <ligne de commande>"
#define DETAIL_RET_SEND_INPUT_SYNTAX      CMD_SEND_INPUT " <id> <input>"
#define DETAIL_RET_GET_OUTPUT_SYNTAX      CMD_GET_OUTPUT " <id>"
#define DETAIL_RET_GET_RETURN_CODE_SYNTAX CMD_GET_RETURN_CODE " <id>"

#define DETAIL_RET_CREATE_PROCESS_ERROR  "Impossible de créer le processus"
#define DETAIL_RET_SEND_INPUT_ERROR      "Impossible d'envoyer sur l'entrée standard du processus"
#define DETAIL_RET_GET_OUTPUT_ERROR      "Impossible de récupérer la sortie standard du processus"
#define DETAIL_RET_GET_RETURN_CODE_ERROR "Impossible de récupérer le code de retour du processus"

#define DETAIL_RET_UNKNOWN_COMMAND "Commande inconnue"
#define DETAIL_RET_UNKNOWN_PROCESS "PID inconnu"

/* Divers */
#define WELCOME_MSG "Welcome on CaDi daemon"    /* Quand un client se connecte */
#define VERBOSE_FLAG 1                          /* différent de 0 si on veut du verbose, 0 sinon */

/* Contient les informations sur un socket (son id et sa structure de connection) */
typedef struct {
    int socket;
    struct sockaddr_in address;
} socketinfo_t;

#endif
