#ifndef CADID_H
#define CADID_H

#define SERVER_BUFFER_SIZE 100
#define SERVER_PORT        53553

#define MSG_QUIT            0
#define MSG_ERR             1
#define MSG_OK              2
#define MSG_UNKNOWN_COMMAND 3

#define VERBOSE_FLAG 1 /* différent de 0 si on veut du verbose, 0 sinon */

typedef struct {
  int socket;
  struct sockaddr_in address;
} socketinfo_t;


/* Destiné à la suppression sans doute ? */
#define MAX_CLIENT         10

#endif
