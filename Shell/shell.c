#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "command.h"
#include "single_command.h"

command_t *g_current_command = NULL;
single_command_t *g_current_single_command = NULL;
char g_path[PATH_MAX];
int yyparse(void);
void yyrestart(FILE*);


/*
 * Interrupt handler
 */

extern void sig_int_handler() {
  printf("\n");
  print_prompt();
} /* sig_int_handler() */

/*
 * Sigchild handler for ctrl_c
 */

extern void sig_child_handler(int sig, siginfo_t *info, void *ucontext) {
  sig = sig;
  ucontext = ucontext;
  int pid = info->si_pid;

  waitpid(pid, NULL, WNOHANG);
  if (pid == get_last_background_pid()) {
    fprintf(stdout, "[%d] exited.\n", pid);
  }
} /* sig_child_handler() */

/*
 * Get the path of the program
 */

char* get_path() {
  return realpath(g_path, NULL);
} /* get_path() */

/*
 *  Prints shell prompt
 */

void print_prompt() {
  if (isatty(0)) {
    printf("myshell>");
  }

  struct sigaction signal_action = {0};
  signal_action.sa_handler = sig_int_handler;
  sigemptyset(&signal_action.sa_mask);
  signal_action.sa_flags = SA_RESTART;

  struct sigaction sa = {0};
  sa.sa_sigaction = sig_child_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  sigaction(SIGCHLD, &sa, NULL);


  int error = sigaction(SIGINT, &signal_action, NULL);
  if (error) {
    perror("sigaction");
    exit(-1);
  }
  fflush(stdout);
} /* print_prompt() */

/*
 *  This main is simply an entry point for the program which sets up
 *  memory for the rest of the program and the turns control over to
 *  yyparse and never returns
 */

int main(int argc, char* argv[]) {
  g_current_command = (command_t *)malloc(sizeof(command_t));
  g_current_single_command =
        (single_command_t *)malloc(sizeof(single_command_t));
  strcpy(g_path, argv[0]);
  argc = argc;
  create_command(g_current_command);
  create_single_command(g_current_single_command);
  FILE* fd = fopen(".shellrc", "r");
  if (fd) {
    yyrestart(fd);
    yyparse();
    yyrestart(stdin);
    fclose(fd);
  }
  else {
    print_prompt();
  }
  yyparse();
} /* main() */
