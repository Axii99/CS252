#ifndef SHELL_H
#define SHELL_H

#include "command.h"
#include "single_command.h"

void print_prompt();
char* get_path();

extern command_t *g_current_command;
extern single_command_t *g_current_single_command;

#endif
