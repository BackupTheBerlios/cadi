#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "process.h"

/** La structure utilisée en interne pour contenir la liste des pids et leurs sorties */
typedef struct {
  pid_t pid;
  char *out;
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
static int index_of_process(pid_t pid) {
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
static int get_new_process_index() {
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
static processinfo_t *add_process(pid_t pid) {
  int proc_index;

  proc_index = get_new_process_index();
  if (proc_index < 0)
    return NULL;

  processes[proc_index].out = malloc(STDOUT_BUFFER_SIZE);
  if (processes[proc_index].out == NULL) {
    perror("malloc");
    return NULL;
  }

  processes[proc_index].pid = pid;

  return &processes[proc_index];
}

/**
 * Supprime le process avec le pid spécifié. Si le pid n'existe pas dans la liste, ne fait rien.
 *
 * @param pid pid à killer
 * @public
 */
void del_process(pid_t pid) {
  int index;

  index = index_of_process(pid);
  if (index < 0)
    return;

  processes[index].pid = 0;
  free(processes[index].out);
  processes[index].out = NULL;
}


pid_t create_process(const char *app) {
  pid_t proc;
  processinfo_t *procinfo;

  if (app == NULL || (procinfo = add_process(0)) == NULL)
    return 0;

  proc = fork();

  /* Père, on renvoie le pid du fils (qui sera le truc exécuté) */
  if (proc > 0)
    return procinfo->pid = proc;

  /* Fils qui se transforme en process que l'utilisateur désire */
  if (proc == 0) 
    if (execlp(app, NULL) < 0)
      return 0;

  /* Erreur, ne devrait jamais arriver (99.9%) */
  return 0;
}
