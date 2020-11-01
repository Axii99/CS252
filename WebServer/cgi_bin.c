#include "cgi_bin.h"

#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <link.h>

#define MAX_URI_LEN (1024)
#define MAX_MOD_NUM (20)

#include "http_messages.h"
#include "socket.h"

extern char **environ;
typedef void (*httprunfunc) (int ssock, const char *query_string);
char** g_loaded_mods = NULL;
int g_mods_num = 0;

/*
 * add loaded mods
 */

void add_mods(char* mod_name) {
  if (g_loaded_mods == NULL) {
    g_loaded_mods = malloc(MAX_MOD_NUM * sizeof(char*));
  }
  g_loaded_mods[g_mods_num] = malloc(sizeof(char) * strlen(mod_name) + 1);
  strcpy(g_loaded_mods[g_mods_num], mod_name);
  g_mods_num++;
} /* add_mods() */

/*
 * Check if this mod is loaded
 */

int is_loaded(char* uri) {
  for (int i = 0; i < g_mods_num; i++) {
    if (!strcmp(g_loaded_mods[i], uri)) {
      return 1;
    }
  }
  return 0;
} /* is_loaded() */

/*
 *  parse uri and query
 */

char* parse_uri(char* uri) {
  int i = 0;
  for (i = 0; i < strlen(uri); i++) {
    if (uri[i] == '?') {
      break;
    }
  }
  if (i == strlen(uri)) {
    return NULL;
  }
  int len = strlen(uri);
  char* query = malloc(sizeof(char) * (len - i));
  strncpy(query, uri + i + 1, len - i - 1);
  query[len - i] = '\0';
  uri[i] = '\0';
  printf("U: %s\nQ: %s\n", uri, query);
  return query;
} /* parse_uri() */

/*
 * so modules
 */

void handle_module(char* uri, char* query, int fd) {
  void *lib = NULL;
  lib = dlopen(uri, RTLD_LAZY);
  if (lib == NULL) {
    fprintf(stderr, "not found");
    perror("dlopen");
    exit(1);
  }
  add_mods(uri);
  httprunfunc mod_httprun;
  mod_httprun = (httprunfunc) dlsym(lib, "httprun");
  if (mod_httprun == NULL) {
    perror("dlsym");
    exit(1);
  }
  mod_httprun(fd, query);
}

/*
 * Handle a CGI request
 */

void handle_cgi_bin(http_request *request, http_response *response, socket_t *sock) {
  char uri[MAX_URI_LEN] = {0};
  strcpy(uri, "./http-root-dir");
  strcat(uri, request->request_uri);
  char* query = parse_uri(uri);

  if (strstr(uri,".so")) {
    handle_module(uri, query, sock->socket_fd);
    return;
  }

  if (query) {
    setenv("REQUEST_METHOD", "GET", 1);
    setenv("QUERY_STRING", query, 1);
  }
  // TODO: Implement this
  int pipe_fd[2];
  pipe(pipe_fd);
  int tmpout = dup(1);
  dup2(pipe_fd[0], 0);
  dup2(pipe_fd[1], 1);
  int ret = 0;
  ret = fork();
  if (ret == 0) {
    char* args[2];
    args[0] = uri;
    args[1] = NULL;
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    execvp(uri, args);
    exit(1);
  }
  close(pipe_fd[1]);
  dup2(tmpout, 1);
  char ch;
  char result[2000];
  int i = 0;
  while (read(0, &ch, 1) > 0) {
    result[i] = ch;
    i++;
  }
  result[i] = '\0';
  socket_write(sock, result, strlen(result));
  close(tmpout);
  waitpid(ret, NULL, 0);
  return;
} /* handle_cgi_bin() */
