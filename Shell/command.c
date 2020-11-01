/*
 * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 * DO NOT PUT THIS PROJECT IN A PUBLIC REPOSITORY LIKE GIT. IF YOU WANT
 * TO MAKE IT PUBLICALLY AVAILABLE YOU NEED TO REMOVE ANY SKELETON CODE
 * AND REWRITE YOUR PROJECT SO IT IMPLEMENTS FUNCTIONALITY DIFFERENT THAN
 * WHAT IS SPECIFIED IN THE HANDOUT. WE OFTEN REUSE PART OF THE PROJECTS FROM
 * SEMESTER TO SEMESTER AND PUTTING YOUR CODE IN A PUBLIC REPOSITORY
 * MAY FACILITATE ACADEMIC DISHONESTY.
 */

#include "command.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "shell.h"

extern char **environ;

int g_status = 0;
int g_last_background_pid = 0;
char g_last_arg[1024];
int g_exit_code = 0;

/*
 * return the last argument
 */

char* get_last_arg() {
  return strdup(g_last_arg);
} /* get_last_arg() */

/*
 * return the exit code
 */

int get_exit_code() {
  return g_exit_code;
} /* get_exit_code() */

/*
 * return the last background pid
 */

int get_last_background_pid() {
  return g_last_background_pid;
} /* get_last_background_pid() */

/*
 *  Initialize a command_t
 */

void create_command(command_t *command) {
  command->single_commands = NULL;

  command->out_file = NULL;
  command->in_file = NULL;
  command->err_file = NULL;

  command->append_out = false;
  command->append_err = false;
  command->background = false;

  command->num_single_commands = 0;
} /* create_command() */

/*
 *  Insert a single command into the list of single commands in a command_t
 */

void insert_single_command(command_t *command, single_command_t *simp) {
  if (simp == NULL) {
    return;
  }

  command->num_single_commands++;
  int new_size = command->num_single_commands * sizeof(single_command_t *);
  command->single_commands = (single_command_t **)
                              realloc(command->single_commands,
                                      new_size);
  command->single_commands[command->num_single_commands - 1] = simp;
} /* insert_single_command() */

/*
 *  Free a command and its contents
 */

void free_command(command_t *command) {
  for (int i = 0; i < command->num_single_commands; i++) {
    free_single_command(command->single_commands[i]);
  }
  free(command->single_commands);
  if (command->out_file) {
    free(command->out_file);
    command->out_file = NULL;
  }

  if (command->in_file) {
    free(command->in_file);
    command->in_file = NULL;
  }

  if (command->err_file) {
    free(command->err_file);
    command->err_file = NULL;
  }

  command->append_out = false;
  command->append_err = false;
  command->background = false;

  free(command);
  command = NULL;
} /* free_command() */

/*
 *  Print the contents of the command in a pretty way
 */

void print_command(command_t *command) {
  printf("\n\n");
  printf("              COMMAND TABLE                \n");
  printf("\n");
  printf("  #   single Commands\n");
  printf("  --- ----------------------------------------------------------\n");

  // iterate over the single commands and print them nicely
  for (int i = 0; i < command->num_single_commands; i++) {
    printf("  %-3d ", i );
    print_single_command(command->single_commands[i]);
  }

  printf( "\n\n" );
  printf( "  Output       Input        Error        Background\n" );
  printf( "  ------------ ------------ ------------ ------------\n" );
  printf( "  %-12s %-12s %-12s %-12s\n",
            command->out_file?command->out_file:"default",
            command->in_file?command->in_file:"default",
            command->err_file?command->err_file:"default",
            command->background?"YES":"NO");
  printf( "\n\n" );
} /* print_command() */

/*
 * argument escaping
 */

char* escaping(char* arg) {
  char *escaped = (char*) malloc(strlen(arg) + 1);
  char *tmp_a = arg;
  char *tmp_e = escaped;
  while (*tmp_a) {
    if (*tmp_a == '\\') {
      tmp_a++;
      *tmp_e = *tmp_a;
      tmp_e++;
      tmp_a++;
    }
    else {
      *tmp_e = *tmp_a;
      tmp_e++;
      tmp_a++;
    }
  }
  *tmp_e = '\0';
  return escaped;
} /* escaping() */

/*
 *  Execute a command
 */

void execute_command(command_t *command) {
  // Don't do anything if there are no single commands
  if ((command->single_commands == NULL) && (isatty(0))) {
    print_prompt();
    return;
  }

  // Print contents of Command data structure
//  print_command(command);

  // Add execution here
  // For every single command fork a new process
  // Setup i/o redirection
  // and call exec
  int tmpin = dup(0);
  int tmpout = dup(1);
  int tmperr = dup(2);
  int fdin = 0;
  int fderr = 0;
  if (command->in_file) {
    fdin = open(command->in_file, O_RDONLY);
  }
  else {
    fdin = dup(tmpin);
  }

  if (command->err_file) {
    if (command->append_err == 1) {
      fderr = open(command->err_file, O_CREAT | O_WRONLY | O_APPEND, 0666);
    }
    else {
      fderr = open(command->err_file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    }
  }
  else {
    fderr = dup(tmperr);
  }

  int ret = 0;
  int fdout = 0;
  for (int i = 0; i < command->num_single_commands; i++) {

    dup2(fdin, 0);
    close(fdin);
    dup2(fderr, 2);
    close(fderr);
    if (i == command->num_single_commands - 1) {
      int num = command->single_commands[i]->num_args;
      strcpy(g_last_arg, command->single_commands[i]->arguments[num - 1]);
      if (command->out_file) {
        if (command->append_out == 1) {
          fdout = open(command->out_file, O_CREAT | O_WRONLY | O_APPEND, 0666);
        }
        else {
          fdout = open(command->out_file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
        }
      }
      else {
        fdout = dup(tmpout);
      }
    }
    else {
      int fdpipe[2];
      pipe(fdpipe);
      fdout = fdpipe[1];
      fdin = fdpipe[0];
    }
    dup2(fdout, 1);
    close(fdout);

    if (!strcmp(command->single_commands[i]->arguments[0], "cd")) {
      if (command->single_commands[i]->num_args == 1) {
        chdir(getenv("HOME"));
      }
      else {
        int check = chdir(command->single_commands[i]->arguments[1]);
        if (check == -1) {
          fprintf(stderr, "cd: can't cd to %s\n",
                  command->single_commands[i]->arguments[1]);
        }
      }
      continue;
    }

    if (!strcmp(command->single_commands[i]->arguments[0], "setenv")) {
      setenv(command->single_commands[i]->arguments[1],
              command->single_commands[i]->arguments[2], 1);
      continue;
    }
    if (!strcmp(command->single_commands[i]->arguments[0], "unsetenv")) {
      unsetenv(command->single_commands[i]->arguments[1]);
      continue;
    }

    ret = fork();
    if (ret == 0) {
      if (!strcmp(command->single_commands[i]->arguments[0], "printenv")) {
        char **p = environ;
        while (*p != NULL) {
          printf("%s\n", *p++);
        }
        exit(0);
      }
      //child
      insert_argument(command->single_commands[i], NULL);
      execvp(command->single_commands[i]->arguments[0],
              command->single_commands[i]->arguments);
      perror("execvp");
      exit(1);

    }
    else {
    }
    //parent shell continue
  }

  dup2(tmpin, 0);
  dup2(tmpout, 1);
  dup2(tmperr, 2);
  close(tmpin);
  close(tmpout);
  close(tmperr);
  if (!command->background) {
    waitpid(ret, &g_exit_code, 0);
    g_exit_code = WEXITSTATUS(g_exit_code);
  }
  else {
    g_last_background_pid = ret;
  }
  // Clear to prepare for next command
  free_command(command);

  // Print new prompt
  if (isatty(0)) {
    print_prompt();
  }
} /* execute_command() */
