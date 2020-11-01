
/*
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

%code requires 
{

}

%union
{
  char * string;
}

%token <string> WORD PIPE
%token NOTOKEN NEWLINE STDOUT APPEND_STDOUT BACKGROUND STDIN STDERR APPEND_STDERR

%{

#include <stdbool.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <regex.h>
#include <dirent.h>

#include "command.h"
#include "single_command.h"
#include "shell.h"

void yyerror(const char * s);
int yylex();
char* env_expansion(char*);
extern int last_background_pid;
void expand_wildcards(char*, char*);
int compare(const void*, const void*);
int is_wildcards(char*);
int max_entries = 20;
int num_entries = 0;
char** array = NULL;
%}

%%

goal:
  entire_command_list
  ;

entire_command_list:
     entire_command_list entire_command {
       execute_command(g_current_command);
       g_current_command = malloc(sizeof(command_t));
       create_command(g_current_command);
     }
  |  entire_command {
       execute_command(g_current_command);
       g_current_command = malloc(sizeof(command_t));
       create_command(g_current_command);
      }
  ;

entire_command:
     single_command_list io_modifier_list background NEWLINE
  |  NEWLINE    
  ;

single_command_list:
     single_command_list PIPE single_command
  {
  } 
  |  single_command
  ;

single_command:
    executable argument_list {
      insert_single_command(g_current_command, g_current_single_command);
      g_current_single_command = NULL;
    }
  ;

argument_list:
     argument_list argument {
     }
     
  |  /* can be empty */
  ;

argument:
     WORD {
        char* arg = escaping($1);
        arg = env_expansion(arg);
        if (!is_wildcards(arg)) {
          insert_argument(g_current_single_command, arg);
        }
        else {
          max_entries = 20;
          num_entries = 0; 
          array = (char**) malloc(max_entries * sizeof(char*));
          expand_wildcards(NULL, arg);      
          if (num_entries) {
            qsort(array, num_entries, sizeof(char*), compare);
            for (int i = 0; i < num_entries; i++) {
              insert_argument(g_current_single_command, strdup(array[i]));
            }
            free(array);
            array = NULL;
          }
          else {
            insert_argument(g_current_single_command, arg);
          }
        }
    }
  ;

executable:
     WORD {
      if (!strcmp($1,"exit")) {
        exit(0);
      }
      g_current_single_command = malloc(sizeof(single_command_t));
      create_single_command(g_current_single_command);
      insert_argument(g_current_single_command, strdup($1));
     }
  ;

io_modifier_list:
     io_modifier_list io_modifier
  |  /* can be empty */   
  ;

io_modifier:
     STDOUT WORD{
      if (g_current_command->out_file != NULL){
        printf("Ambiguous output redirect.\n");
      }
      g_current_command->out_file = strdup($2);
    }
  | STDIN WORD {g_current_command->in_file = strdup($2);}
  | STDERR WORD {g_current_command->err_file = strdup($2);}
  | APPEND_STDOUT WORD {g_current_command->append_out = true;
    g_current_command->out_file = strdup($2);}
  | APPEND_STDERR WORD {g_current_command->append_err = true;
    g_current_command->err_file = strdup($2);}
  ;

background:
     BACKGROUND 
      {g_current_command->background = 1;}
  | /*empoty*/
  ;


%%


char* get_home_path() {
  char* path = getenv("HOME");
  for(int i = 1; i < strlen(path); i ++) {
    if (path[i] == '/') {
      path[++i] = '\0';
      break;
    }
  }
  return path;
}

char* env_expansion(char* arg) {
  char* env_expand = (char*)calloc(4096, sizeof(char));
  int n = 0;
  for (int i = 0; i < strlen(arg); i++) {
    if (i == strlen(arg) - 1) {
      env_expand[n] = arg[i];
      n++;
      break;
    }
    if (arg[i] == '$' && arg[i + 1] == '{') {
      int j = i + 1;
      while (j < strlen(arg)) {
        if (arg[j] == '}') {
          char* substring = (char*)calloc(j - i, sizeof(char));
          strncpy(substring, &arg[i + 2], j - i - 2);
          char* to_replace = (char*) malloc(2048);
          if (!strcmp(substring, "$")) {
            int pid = getpid();
            sprintf(to_replace, "%d", pid);
          }
          else if (!strcmp(substring, "?")) {
            sprintf(to_replace, "%d", get_exit_code());
          }
          else if (!strcmp(substring, "!")) {
            sprintf(to_replace, "%d", get_last_background_pid());
          }
          else if (!strcmp(substring, "_")) {
            char* last_arg = get_last_arg();
            if (last_arg != NULL) {
              to_replace = strdup(last_arg);
            }
          }
          else if (!strcmp(substring, "SHELL")){
            to_replace = get_path();
          }
          else {
            to_replace = getenv(substring);
          }

          if (to_replace != NULL) {
            for (int k = 0; k < strlen(to_replace); k++) {
              env_expand[n] = to_replace[k];
              n++;
            }
          }
          i = j;
          break;
        }
        j++;
      }
    }
    else {
      env_expand[n] = arg[i];
      n++;
    }
  }
  env_expand[n] = '\0';
  //tilde expansion
  if (env_expand[0] == '~') {
    if (strlen(env_expand) == 1) {
      env_expand = strdup(getenv("HOME"));
    }
    else {
      char* tilde = (char*)malloc(PATH_MAX);
      char* home_path = get_home_path();
      int i = 0;
      for (i = 0; i < strlen(home_path); i++) {
        tilde[i] = home_path[i];
      }
      for (int j = 1; j < strlen(env_expand); j++) {
        tilde[i] = env_expand[j];
        i++;
      }
      tilde[i] = '\0';
      return tilde;
    }
  }
  return env_expand;
}

int compare(const void *first, const void *second) {
  const char* file1 = *(const char**) first;
  const char* file2 = *(const char**) second;
  return strcmp(file1, file2);
}

int is_wildcards(char* arg) {
  if ((strchr(arg, '?')) || (strchr(arg, '*'))) {
    return 1;
  }
  return 0;
}

#define MAXFILENAME 1024

void expand_wildcards(char* prefix, char* arg) {
  if (arg[0] == 0) {
    array[num_entries++] = strdup(prefix);
    return;
  }
  char *s = strchr(arg, '/');
  char component[MAXFILENAME] = "";
  if (s != NULL) {
    strncpy(component, arg, s - arg);
    component[s - arg] = '\0';
    arg = s + 1;
  }
  else {
    strcpy(component, arg);
    arg = arg + strlen(arg);
  }

  char new_prefix[MAXFILENAME];
  if (!is_wildcards(component)) {
    if (prefix) {
      sprintf(new_prefix, "%s/%s", prefix, component);
    }
    else {
      strcpy(new_prefix, component);
    }
    expand_wildcards(new_prefix, arg);
    return;
  }


  char* regex = (char*) malloc(2 * strlen(component) + 3);
  char* arg_pos = component;
  char* regex_pos = regex;

  *regex_pos++ = '^';
  while (*arg_pos) {
    if (*arg_pos == '*') {
      *regex_pos++ = '.';
      *regex_pos++ = '*';
    }
    else if (*arg_pos == '?') {
      *regex_pos++ = '.';
    }
    else if (*arg_pos == '.') {
      *regex_pos++ = '\\';
      *regex_pos++ = '.';
    }
    else {
      *regex_pos++ = *arg_pos;
    }
    arg_pos++;
  }
  *regex_pos++ = '$';
  *regex_pos = '\0';

  regex_t re = {0};
  int status = regcomp(&re, regex, REG_EXTENDED|REG_NOSUB);
  if (status != 0) {
    perror("compile");
    return;
  }

  char* directory = NULL;
  if (prefix != NULL) {
    directory = strdup(prefix);
  }
  else {
    directory = ".";
  }
  DIR* dir = opendir(directory);
  if (dir == NULL) {
    perror("opendir");
    return;
  }
  struct dirent *ent = NULL;

  while ((ent = readdir(dir)) != NULL) {
    regmatch_t match;
    if (regexec(&re, ent->d_name, 1, &match, 0) == 0) {
      if (*arg) {
        if (ent->d_type == DT_DIR) {
          if (!strcmp(directory, ".")) {
            strcpy(new_prefix, ent->d_name);
          }
          else if (!strcmp(directory, "/")) {
            sprintf(new_prefix, "%s%s", directory, ent->d_name);
          }
          else {
            sprintf(new_prefix, "%s/%s", directory, ent->d_name);
          }
          expand_wildcards(new_prefix, arg);
        }
      }
      else {
        if (num_entries == max_entries) {
          max_entries *= 2;
          array = realloc(array, max_entries * sizeof(char*));
          assert(array != NULL);
        }
        if (prefix) {
          sprintf(new_prefix, "%s/%s", prefix, ent->d_name);
        }
        else {
          strcpy(new_prefix, ent->d_name);
        }

        if (ent->d_name[0] == '.') {
          if (component[0] == '.') {
            //expand_wildcards(new_prefix, arg);
            array[num_entries++] = strdup(new_prefix);
          }
        }
        else {
          //expand_wildcards(new_prefix, arg);
          array[num_entries++] = strdup(new_prefix);
        }
      }
    }
  }
  closedir(dir);

}

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
  yyparse();
}

#if 0
main(int argc, char* argv[])
{
  yyparse();
}
#endif
