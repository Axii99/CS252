/*
 *
 * CS-252 
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "y.tab.h"


//////////start added code///////////

extern  char * read_line();

int mygetc(FILE * f) {
  static char *p;
  char ch;

  if (!isatty(0)) {
    return getc(f);
  }

  if (p == NULL || *p == 0) {
    char *s = read_line();
    p = s;
  }
  ch = *p;
  p++;
  return ch;
}
#undef getc
#define getc(f) mygetc(f)

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}
#define MAX_STACK_DEPTH 50
YY_BUFFER_STATE stack[MAX_STACK_DEPTH];
int stack_ptr = 0;

%}

%option noyywrap

%%

\n {
  return NEWLINE;
}


[ \t] {
  /* Discard spaces and tabs */
}

">" {
  return STDOUT;
}

"<" {
  return STDIN;
}

">>" {
  return APPEND_STDOUT;
}

">&"|"2>" {
  return STDERR;
}

">>&" {
  return APPEND_STDERR;
}

"|" {
  return PIPE;
}

"&" {
  return BACKGROUND;
}

\$\([^\t\n]*\) {
  char* subcommand = (char*)calloc(strlen(yytext)-3, sizeof(char));
  strncpy(subcommand, &yytext[2], strlen(yytext)-3);
  
  int pin[2];
  int pout[2];
  pipe(pin);
  pipe(pout);

  int tmpin = dup(0);
  int tmpout = dup(1);


  write(pin[1], subcommand, strlen(subcommand)+1);    
  write(pin[1], "\n", 1);
  write(pin[1], "exit", 4);
  write(pin[1], "\n", 1);

  close(pin[1]);

  dup2(pin[0], 0);
  dup2(pout[1], 1);
  close(pin[0]);
  close(pout[1]);
  
  int ret = fork();
  if (ret == 0) {
    yyparse();
    exit(0);
  }
  else {
  }
  waitpid(ret, NULL, 0);
  dup2(tmpin, 0);
  dup2(tmpout, 1);
  close(tmpin);
  close(tmpout);
  char read_char;
  char *fed_back = (char*) malloc(2048);
  int i = 0;
  while (read(pout[0], &read_char, 1) > 0) {
     if (read_char == '\n') {
        fed_back[i] = ' ';
    }
    else {
      fed_back[i] = read_char;
    }
    i++;
  }
  fed_back[i] = '\0';
  for (int j = strlen(fed_back) - 1; j >= 0; j--) {
    myunputc(fed_back[j]);
  }
  

  
}

source[^\t\n]+ {
  yylval.string = (char*)calloc(strlen(yytext)-6, sizeof(char));
  strncpy(yylval.string, &yytext[7], strlen(yytext)-1);
  if (stack_ptr >= MAX_STACK_DEPTH) {
    fprintf(stderr, "too deep");
    exit(1);  
  }
  
  stack[stack_ptr++] = YY_CURRENT_BUFFER;
  yyin = fopen(yylval.string, "r");
  if (!yyin) {
  }
  yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
  BEGIN(INITIAL);
  
}

<<EOF>> {
  if (--stack_ptr < 0) {
    yyterminate(); 
  }
  else {
    yy_delete_buffer(YY_CURRENT_BUFFER);
    yy_switch_to_buffer(stack[stack_ptr]);
  }
}

[^ \t\n]*\\> {
  /* Assume that file names have only alpha chars */
  yylval.string = strdup(yytext);
  return WORD;
}

[^ \t\n]*\\< {
  /* Assume that file names have only alpha chars */
  yylval.string = strdup(yytext);
  return WORD;
}

[^ \t\n]*\\& {
  /* Assume that file names have only alpha chars */
  yylval.string = strdup(yytext);
  return WORD;
}

\"(\\.|[^"\\])*\" {
  yylval.string = (char*)calloc(strlen(yytext)-1, sizeof(char));
  strncpy(yylval.string, &yytext[1],strlen(yytext)-2);
  return WORD;
}

[^ \t\n\|><&]+ {
  /* Assume that file names have only alpha chars */
  yylval.string = strdup(yytext);
  return WORD;
}


