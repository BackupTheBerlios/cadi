#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include "process.h"
#include "cadid.h"

#define WRITE 1
#define READ  0

/** La structure utilisée en interne pour contenir les infos d'un processus */
typedef struct {
  pid_t pid;
  int ret;
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

    processes[proc_index].ret = PROCESS_NOT_TERMINATED;

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

pid_t create_process(const char *prog, char * const args[])
{
    pid_t proc;
    processinfo_t *procinfo;

    if (prog == NULL || (procinfo = add_process()) == NULL)
        return 0;

    proc = fork();

    if (proc > 0) {
      close(procinfo->in[READ]);
      close(procinfo->out[WRITE]);
      fcntl(procinfo->out[READ], F_SETFL, O_NONBLOCK);
      return procinfo->pid = proc;
    }
    
    /* Le fils, le processus exécuté */
    if (proc == 0) {
      /*dup2(procinfo->in[READ], STDIN_FILENO);
      close(procinfo->in[READ]);
      close(procinfo->in[WRITE]);*/

      dup2(procinfo->out[WRITE], STDOUT_FILENO);
      close(procinfo->out[READ]);
      close(procinfo->out[WRITE]);

      printf("saluiam\n");
      printf("loooooool");
      fflush(stdout);

      /*execlp("tr", "tr", "[:lower:]", "[:upper:]", NULL);*/
      execvp(prog, args);
      return 0;
    }

    /* Erreur, ne devrait jamais arriver (99.9999%) */
    perror("fork");
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
  char *buf;

  if ((index = index_of_process(pid)) < 0)
    return;

  /* Le TRUC à ne pas oublier !
     Enfin, ou trouver une autre technique, genre un remplacement barbare des "\n"
     du SendInput par le vrai '\n', un "<< EOF", etc. */
  if ((buf = malloc(strlen(input) + 1 + 1)) == NULL) {
    perror("malloc");
    return;
  }

  strcpy(buf, input);
  strcat(buf, "\n");
  
  write(processes[index].in[WRITE], buf, strlen(buf) + 1);
}

void get_output(int socket, pid_t pid) {
  static char buffer[STDOUT_BUFFER_SIZE];
  int index, n;
  bool must_n = false;

  index = index_of_process(pid);
  if (index < 0)
    return;

  while ((n = read(processes[index].out[READ], buffer, sizeof buffer - 1)) > 0) {
    buffer[n] = '\0';
    send_basic(socket, buffer);
    must_n = true;
  }

  if (must_n)
    send_basic(socket, "\n");
}

int get_return_code(pid_t pid) {
  int index;

  index = index_of_process(pid);
  if (index < 0)
    return PROCESS_NOT_TERMINATED; /* N'arrivera normalement jamais */

  return processes[index].ret;
}
