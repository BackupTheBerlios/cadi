#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "cadid.h"

int main() {
  struct sockaddr_in server_address;
  int server_socket;
  char buffer[SERVER_BUFFER_SIZE];
  char *p;
  pid_t pid;

  if ((server_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("ouverture du socket");
    return EXIT_FAILURE;
  }

  memset(&server_address, 0, sizeof server_address);
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(SERVER_PORT);

  if (connect(server_socket, (struct sockaddr *) &(server_address), sizeof server_address) < 0) {
    perror("connect");
    return EXIT_FAILURE;
  }

  pid = fork();

  /** Plantage :D */

  if (pid < 0) {
    perror("fork");
    return EXIT_FAILURE;
  } 

  /* Père */

  if (pid > 0) {
    while (recv(server_socket, buffer, SERVER_BUFFER_SIZE, 0) > 0) {
      printf("%s", buffer);
      fflush(stdout);
    }

    return EXIT_SUCCESS;
  }

  /* Fils */

  while (fgets(buffer, SERVER_BUFFER_SIZE, stdin)) {

    if ((p = strchr(buffer, 10)))
      *p = '\0';

    if (send(server_socket, buffer, SERVER_BUFFER_SIZE, 0) < 0) {
      perror("send");
      return EXIT_FAILURE;
    }

  }

  return EXIT_SUCCESS;
}
