#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "process.h"

FILE *fdopen (int fildes, const char *mode);

#define IN  1
#define OUT 0

/** La structure utilisée en interne pour contenir les infos d'un processus */
typedef struct {
  pid_t pid;
  int rc;
  int in[2];
  int out[2];
} processinfo_t;

/** Liste des processus */
static processinfo_t processes[MAX_PROCESS];

/**
 * Retourne l'index du process ayant un pid précis.
 *
 * @param pid un pid
 * @return -1 si le pid n'est pas enregistré, l'index sinon
 * @private
 */
static int index_of_process(pid_t pid)
{
    int i;

    for (i = 0; i < MAX_PROCESS; i++)
        if (processes[i].pid == pid)
            return i;

    return -1;
}

/**
 * Retourne l'index où l'insertion d'un nouveau process est possible.
 *
 * @return -1 s'il n'y a plus de place, l'index sinon
 * @private
 */
static int get_new_process_index()
{
    int i;

    for (i = 0; i < MAX_PROCESS; i++)
        if (!processes[i].pid)
            return i;

    return -1;
}

/**
 * Ajoute un nouveau process dans la liste avec un pid précisé.
 *
 * @param pid pid du process
 * @return 0 si erreur, 1 sinon
 * @private
 */
static processinfo_t *add_process()
{
    int proc_index;

    proc_index = get_new_process_index();
    if (proc_index < 0)
        return NULL;

    processes[proc_index].rc = PROCESS_NOT_TERMINATED;
    pipe(processes[proc_index].in);
    pipe(processes[proc_index].out);

    return &processes[proc_index];
}

/**
 * Supprime le process avec le pid spécifié. Si le pid n'existe pas dans la liste, ne fait rien.
 *
 * @param pid pid à killer
 * @public
 */
void destroy_process(pid_t pid)
{
    int index, kill(pid_t pid, int sig);

    index = index_of_process(pid);
    if (index < 0)
        return;

    if (kill(processes[index].pid, SIGTERM))
      /* Le SIGTERM ferme pas le processus ? ON KILL J'AI DIS */
      if (kill(processes[index].pid, SIGKILL))
        perror("Impossible de détruire le processus");

    processes[index].pid = 0;
}

pid_t create_process(const char *app)
{
    pid_t proc;
    processinfo_t *procinfo;

    if (app == NULL || (procinfo = add_process()) == NULL)
        return 0;

    proc = fork();

    /* Le serveur (père) */
    if (proc > 0) {
      if (write(procinfo->in[IN], "Salut", 6) < 0) {
	perror("write");
      }
      return procinfo->pid = proc;
    }
    
    /* Le fils, le processus exécuté */
    if (proc == 0) {
      FILE *f = fdopen(procinfo->in[IN], "r");

      if (feof(f)) {
	puts("eof");
	fflush(stdout);
      } else {
	char *buffer = malloc(10);
	fgets(buffer, 10, f);
	buffer[10] = '\0';
	printf("%s\n", buffer);
	fflush(stdout);
      }
	
      dup2(procinfo->in[OUT], STDIN_FILENO);
      
      if (execlp("tr", "tr", "[:upper:]", "[:lower:]", NULL) < 0)
	return 0;
    }

    /* Erreur, ne devrait jamais arriver (99.9%) */
    return 0;
}

/**
 * Indique si le processus d'id pid a été crée.
 *
 * @param pid un id
 * @return true si le processus id existe, false sinon
 */
bool process_exists(pid_t pid) {
  return index_of_process(pid) == -1 ? false : true;
}

void send_input(pid_t pid, const char *input) {
  int index;

  index = index_of_process(pid);
  if (index < 0)
    return;

  write(processes[index].in[IN], input, strlen(input) + 1);
}

char *get_output(pid_t pid) {
  static char buffer[STDOUT_BUFFER_SIZE];
  int index, n;

  index = index_of_process(pid);
  if (index < 0)
    return NULL;

  n = read(processes[index].out[OUT], buffer, sizeof buffer);
  return buffer;
}

int get_return_code(pid_t pid) {
  int index;

  index = index_of_process(pid);
  if (index < 0)
    return PROCESS_NOT_TERMINATED; /* N'arrivera jamais */

  return processes[index].rc;
}
