#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>

#include "single_command.h"

// Command Data Structure

typedef struct command {
  char *out_file;
  char *in_file;
  char *err_file;
  bool append_out;
  bool append_err;
  bool background;
  single_command_t **single_commands;
  int num_single_commands;
} command_t;

char* get_last_arg();
int get_exit_code();
int get_last_background_pid();
void create_command(command_t *);
void insert_single_command(command_t *, single_command_t *);
void free_command(command_t *);
void print_command(command_t *);
void execute_command(command_t *);
char* escaping(char *);

extern command_t *g_current_command;

#endif


