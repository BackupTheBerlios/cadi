#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include "process.h"
#include "cadid.h"

#define WRITE 1
#define READ  0

extern int kill(pid_t pid, int sig);

/** La structure utilisée en interne pour contenir les infos d'un processus */
typedef struct
{

  pid_t pid;
  int ret;
  int in[2]; /* parent -> child */
  int out[2]; /* child -> parent */
  int err[2]; /* child -> parent */
  char *command;

} processinfo_t;

/** Liste des processus */
static processinfo_t processes[MAX_PROCESS];

/**
 * Retourne l'index du process ayant un pid précis.
 *
 * @param pid un pid
 * @return -1 si le pid n'est pas enregistré, l'index sinon
 */
static int index_of_process(pid_t pid)
{
  for (unsigned i = 0; i < sizeof processes / sizeof processes[0]; i++)
    if (processes[i].pid == pid)
      return i;
  
  return -1;
}

/**
 * Retourne l'index où l'insertion d'un nouveau process est possible.
 *
 * @return -1 s'il n'y a plus de place, l'index sinon
 */
static int get_next_new_process_index()
{
  for (unsigned i = 0; i < sizeof processes / sizeof processes[0]; i++)
    if (!processes[i].pid)
      return i;
  
  return -1;
}

/**
 * Initialise un nouveau processus.
 */
static processinfo_t *add_process()
{
  int proc_index = get_next_new_process_index();
  if (proc_index == -1)
    return NULL;

  processes[proc_index].ret = PROCESS_NOT_TERMINATED;
  processes[proc_index].command = NULL;
  
  if (pipe(processes[proc_index].in) == -1
      || pipe(processes[proc_index].out) == -1
      || pipe(processes[proc_index].err) == -1)
    {
      perror("pipe");
      return NULL;
    }
  
  return processes + proc_index;
}

void list_process(int socket) {
  char msg[MESSAGE_BUFFER_SIZE];

  /* Header */
  snprintf(msg, sizeof msg, "Ret.\tPID\tCommande\n");
  send_basic(socket, msg, strlen(msg));

  /* Liste */
  for (unsigned i = 0; i < sizeof processes / sizeof processes[0]; i++)
    {
      if (!processes[i].pid)
	continue;
	
      snprintf(msg, sizeof msg, "%3d\t%d\t%s\n", get_return_code(processes[i].pid), processes[i].pid, processes[i].command);
      send_basic(socket, msg, strlen(msg));
    }
}

void destroy_all_process() {
  int i = 0;
  while (processes[i].pid)
    destroy_process(processes[i++].pid);
}

/**
 * Supprime le process avec le pid spécifié. Si le pid n'existe pas dans la liste, ne fait rien.
 *
 * @param pid pid à killer
 */
void destroy_process(pid_t pid)
{
  int index = index_of_process(pid);
  if (index < 0)
    return;

  /* On le tue s'il n'est pas déjà terminé */
  if (get_return_code(pid) == PROCESS_NOT_TERMINATED
      && kill(processes[index].pid, SIGHUP) == -1
      && kill(processes[index].pid, SIGKILL) == -1)
    perror("kill");
  
  close_input(pid);
  close(processes[index].out[READ]);
  close(processes[index].err[READ]);
  free(processes[index].command);

  processes[index].pid = 0;
}

pid_t create_process(const char *prog, char *const args[])
{
  processinfo_t *procinfo;
  
  if (prog == NULL || (procinfo = add_process()) == NULL)
    return -1;
  
  pid_t proc;
  switch (proc = fork()) {
    
  case -1: /* Erreur */
    {
      perror("fork");
      return -1;
    }

  case 0: /* Fils */
    {
      fflush(stdin); fflush(stdout); fflush(stderr);
      setbuf(stdin, NULL); setbuf(stdout, NULL); setbuf(stderr, NULL);

      dup2(procinfo->in[READ], STDIN_FILENO);
      dup2(procinfo->out[WRITE], STDOUT_FILENO);      
      dup2(procinfo->err[WRITE], STDERR_FILENO);
      
      close(procinfo->in[READ]); close(procinfo->in[WRITE]);
      close(procinfo->out[READ]); close(procinfo->out[WRITE]);
      close(procinfo->err[READ]); close(procinfo->err[WRITE]);
      
      execvp(prog, args);
      perror("execvp");
      exit(-1);
    }
    
  default: /* Père */
    {
      close(procinfo->in[READ]);
      close(procinfo->out[WRITE]);
      close(procinfo->err[WRITE]);

      if (fcntl(procinfo->out[READ], F_SETFL, O_NONBLOCK) == -1 ||
	  fcntl(procinfo->err[READ], F_SETFL, O_NONBLOCK) == -1)
	perror("fcntl");
      
      char *cmd = malloc(MESSAGE_BUFFER_SIZE);

      /* Si le malloc a foiré, cmd reste à NULL, donc NP */
      if (cmd != NULL)
	for (int i = 0; args[i] != NULL; i++)
	  {
	    strcat(cmd, args[i]);
	    strcat(cmd, " ");
	  }

      else
	perror("malloc");

      procinfo->command = cmd;

      return procinfo->pid = proc;
    }
  }
}


/**
 * Indique si le processus d'id pid a été crée.
 *
 * @param pid un id
 * @return true si le processus id existe, false sinon
 */
bool process_exists(pid_t pid)
{
  return index_of_process(pid) == -1 ? false : true;
}

void send_input(pid_t pid, const char *input)
{
  int index;
  if ((index = index_of_process(pid)) < 0)
    return;

  char *buf;
  if ((buf = malloc(strlen(input) + 1 + 1)) == NULL)
    {
      perror("malloc");
      return;
    }
  
  sprintf(buf, "%s\n", input);
  
  if (write(processes[index].in[WRITE], buf, strlen(buf)) == -1)
    perror("write");
  
  free(buf);
}

void close_input(pid_t pid)
{
  int index;
  if ((index = index_of_process(pid)) < 0)
    return;

  /* Déjà fermé ? */
  if (processes[index].in[WRITE] == -1)
    return;
  
  /* On ferme sinon ! */
  if (close(processes[index].in[WRITE]) == -1)
    perror("close");
  else 
    /* On le marque comme fermé, pour que input_open le sache */
    processes[index].in[WRITE] = -1;
}

void get_output(int socket, pid_t pid)
{
  int index;
  index = index_of_process(pid);
  if (index < 0)
    return;

  char buffer[STDOUT_BUFFER_SIZE];
  bool must_n = false;
  int n;

  while ((n = read(processes[index].out[READ], buffer, sizeof buffer)) > 0)
    {
      send_basic(socket, buffer, n);
      must_n = true;
    }
  
  if (n == -1)
    perror("read");
  
  if (must_n)
    send_basic(socket, "\n", 1);
}

void get_error(int socket, pid_t pid)
{
  int index = index_of_process(pid);
  if (index < 0)
    return;
  
  char buffer[STDOUT_BUFFER_SIZE];
  int n;
  bool must_n = false;
  
  while ((n = read(processes[index].err[READ], buffer, sizeof buffer)) > 0) {
    send_basic(socket, buffer, n);
    must_n = true;
  }
  
  if (n == -1)
    perror("read");
  
  if (must_n)
    send_basic(socket, "\n", 1);
}

int get_return_code(pid_t pid)
{
  int index = index_of_process(pid);

  if (index < 0) /* N'arrivera normalement jamais */
    return PROCESS_NOT_TERMINATED;

  /*
   * Si on a déjà récup' le rc du process, on le renvoie, sinon, on
   * tente de le récup' 
   */
  if (processes[index].ret == PROCESS_NOT_TERMINATED)
    {
      int status, i;
      
      switch (i = waitpid(pid, &status, WNOHANG))
	{

	case -1: /* On passe en 0 également */
	  perror("waitpid");
	  
	case 0:
	  return PROCESS_NOT_TERMINATED;
	  
        default:
	  processes[index].ret = WEXITSTATUS(status);
        }
    }

  return processes[index].ret;
}

bool input_open(pid_t pid)
{
  int index = index_of_process(pid);

  if (index < 0) /* N'arrivera normalement jamais */
    return false;

  return processes[index].in[WRITE] == -1 ? false : true;
}
