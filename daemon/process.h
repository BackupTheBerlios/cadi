#ifndef PROCESS_H
#define PROCESS_H

/* Nombre de processus maximum */
#define MAX_PROCESS 10

/* Taille du buffer de sortie des process's exécutés */
#define STDOUT_BUFFER_SIZE 2048

void del_process(pid_t);
pid_t create_process(const char *);

#endif
