#ifndef PROCESS_H
#define PROCESS_H

#include <stdbool.h>

/* Nombre de processus maximum */
#define MAX_PROCESS 10

/* Taille du buffer de sortie des process's exécutés */
#define STDOUT_BUFFER_SIZE 2048

/* Le process n'a pas encore retourné */
#define PROCESS_NOT_TERMINATED -1

bool process_exists(pid_t);
void destroy_process(pid_t);
pid_t create_process(const char *);
void send_input(pid_t, const char *);
char *get_output(pid_t);
int get_return_code(pid_t);

#endif
