#ifndef CADID_H
#define CADID_H

#include <netinet/in.h>

#define MESSAGE_BUFFER_SIZE 1024  /* Taille maximale des messages transmis */
#define MAX_ARGS 128 /* Nombre max d'args dans CreateProcess x1 x2 ... xn */

/*
 * Type de la commande reçue 
 */
#define MSG_QUIT            0
#define MSG_ERR             1
#define MSG_OK              2
#define MSG_UNKNOWN_COMMAND 3

/*
 * Commandes
 */
#define CMD_CREATE_PROCESS  "CreateProcess"
#define CMD_DESTROY_PROCESS "DestroyProcess"
#define CMD_SEND_INPUT      "SendInput"
#define CMD_CLOSE_INPUT     "CloseInput"
#define CMD_GET_OUTPUT      "GetOutput"
#define CMD_GET_ERROR       "GetError"
#define CMD_GET_RETURN_CODE "GetReturnCode"
#define CMD_QUIT            "Quit"
#define CMD_GET_HELP        "Help"
#define CMD_LIST_PROCESS    "ListProcess"

/*
 * Retour au client de sa commande 
 */
#define RET_OK  "OK"
#define RET_ERR "ERR"

/*
 * Détail de réponse 
 */
#define DETAIL_RET_QUIT "QUIT"
#define DETAIL_RET_DESTROY_PROCESS_SYNTAX CMD_DESTROY_PROCESS " <id>"
#define DETAIL_RET_CREATE_PROCESS_SYNTAX  CMD_CREATE_PROCESS " <ligne de commande>"
#define DETAIL_RET_SEND_INPUT_SYNTAX      CMD_SEND_INPUT " <id> <input>"
#define DETAIL_RET_CLOSE_INPUT_SYNTAX     CMD_CLOSE_INPUT " <id>"
#define DETAIL_RET_GET_OUTPUT_SYNTAX      CMD_GET_OUTPUT " <id>"
#define DETAIL_RET_GET_ERROR_SYNTAX       CMD_GET_ERROR " <id>"
#define DETAIL_RET_GET_RETURN_CODE_SYNTAX CMD_GET_RETURN_CODE " <id>"
#define DETAIL_RET_LIST_PROCESS_SYNTAX    CMD_LIST_PROCESS

#define DETAIL_RET_CREATE_PROCESS_ERROR  "Impossible de créer le processus"
#define DETAIL_RET_SEND_INPUT_ERROR      "Impossible d'envoyer sur l'entrée standard du processus"
#define DETAIL_RET_CLOSE_INPUT_ERROR     "Impossible de fermer l'entrée standard du processus"
#define DETAIL_RET_GET_OUTPUT_ERROR      "Impossible de récupérer la sortie standard du processus"
#define DETAIL_RET_GET_ERROR_ERROR       "Impossible de récupérer la sortie d'erreur du processus"
#define DETAIL_RET_GET_RETURN_CODE_ERROR "Impossible de récupérer le code de retour du processus"
#define DETAIL_RET_PROCESS_TERMINATED    "Le processus a terminé son exécution"
#define DETAIL_RET_INPUT_CLOSE           "L'entrée standard du processus est fermée"

#define DETAIL_RET_UNKNOWN_COMMAND "Commande inconnue"
#define DETAIL_RET_UNKNOWN_PROCESS "PID inconnu"

extern void send_basic(const int, const void *, unsigned);

#endif
