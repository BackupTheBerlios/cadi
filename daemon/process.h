#ifndef PROCESS_H
#define PROCESS_H

#include <stdbool.h>

/* Nombre de processus maximum */
#define MAX_PROCESS 10

/* Taille du buffer de sortie des process's exécutés */
#define STDOUT_BUFFER_SIZE 10

/* Le process n'a pas encore retourné */
#define PROCESS_NOT_TERMINATED -1

extern bool process_exists(pid_t);
extern void destroy_all_process();
extern void destroy_process(pid_t);
extern pid_t create_process(const char *, char *const[]);
extern void send_input(pid_t, const char *);
extern void close_input(pid_t);
extern void get_output(int socket, pid_t);
extern void get_error(int socket, pid_t);
extern int get_return_code(pid_t);
extern bool input_open(pid_t);
extern void list_process(int);

#endif
