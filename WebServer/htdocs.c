#include "htdocs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>

#include "http_messages.h"
#include "misc.h"
#include "socket.h"

#define MAX_FILE_NAME (1024)

char** g_dirs = NULL;
int g_max_num = 20;
int g_ent_num = 0;

/*
 * Comparison function for sorting
 */

int str_cmp(const void *a, const void *b) {
  const char **str_a = (const char **) a;
  const char **str_b = (const char **) b;
  return strcmp(*str_a, *str_b);
} /* str_cmp() */

/*
 * Call get_content_type and revis the result
 */

char* get_type(char* filename) {
  char* temp = get_content_type(filename);
  if (!temp) {
    return NULL;
  }
  int i = 0;
  for (i = 0; i < strlen(temp); i++) {
    if (temp[i] == ';') {
      break;
    }
  }
  char* type = malloc(sizeof(char) * i);
  strncpy(type, temp, i);
  type[i] = '\0';
  free(temp);
  temp = NULL;
  if (strstr(filename, ".css")) {
    type = "text/css";
  }
  else if (strstr(type, "empty")) {
    type = "text/plain";
  }
  return type;
} /* get_type() */

/*
 * You should implement this function and use it in server.c
 */

int handle_htdocs(http_request *request, http_response *response) {
  char* root_dir = "http-root-dir/htdocs";
  char filename[MAX_FILE_NAME] = {0};
  char* uri = request->request_uri;

  if (!strcmp(uri, "/")) {
    strcat(filename, root_dir);
    strcat(filename, "/index.html");
  }
  else if (strstr(uri, "icons")) {
    strcat(filename, "http-root-dir");
    strcat(filename, uri);
  }
  else if (strstr(uri, "stats")) {
    strcpy(filename, "http-root-dir/myhttpd.stat");
  }
  else if (strstr(uri, "logs")) {
    strcpy(filename, "http-root-dir/myhttpd.log");
  }
  else if (strstr(uri, root_dir)) {
    strcat(filename, uri);
  }
  else {
    strcat(filename, root_dir);
    strcat(filename, uri);
  }
  char* check_type = get_content_type(filename);
  if (check_type == NULL) {
    response->status_code = 404;
    return 0;
  }
  else if (strstr("inode/directory", check_type)) {
    strcat(filename, "/index.html");
    check_type = get_content_type(filename);
    if (check_type == NULL) {
      response->status_code = 404;
      return 0;
    }
  }

  FILE* file = fopen(filename, "rb");
  if (!file) {
    printf("cant open file");
    response->status_code = 403;
    return 0;
  }
  fseek(file, 0, SEEK_END);
  int len = ftell(file);
  fseek(file, 0, SEEK_SET);
  response->message_body = malloc(len * sizeof(char*));
  int ret = fread(response->message_body, 1, len, file);
  if (ret < 0) {
    perror("Read Fail");
  }
  fclose(file);

  response->status_code = 200;
  char* type = get_type(filename);
  response->num_headers = 2;
  response->headers = malloc(sizeof(header) * response->num_headers);
  char* type_key = "Content-Type";
  char* length_key = "Content-Length";
  char length[1024];
  sprintf(length, "%d", len);
  response->headers[0].key = malloc(strlen(type_key) + 1);
  response->headers[1].key = malloc(strlen(length_key) + 1);
  response->headers[0].value = malloc(strlen(type) + 1);
  response->headers[1].value = malloc(strlen(length) + 1);
  strcpy(response->headers[0].key, type_key);
  strcpy(response->headers[1].key, length_key);
  strcpy(response->headers[0].value, type);
  strcpy(response->headers[1].value, length);

  // TODO: Get the request URL, verify the file exists, and serve it
  return 0;
} /* handle_htdocs() */

/*
 * Search the directory
 */

int search_dir(char* uri) {
  char directory[1024];
  sprintf(directory, "./http-root-dir/htdocs%s", uri);
  if (directory[strlen(directory) - 1] != '/') {
    strcat(directory, "/");
  }
  printf("Searching: %s\n", directory);
  DIR* dir = opendir(directory);
  if (dir == NULL) {
    return 1;
  }
  struct dirent *ent = NULL;
  while ((ent = readdir(dir)) != NULL) {
    if (g_ent_num == g_max_num) {
      g_max_num *= 2;
      g_dirs = realloc(g_dirs, g_max_num * sizeof(char*));
    }
    if (!strncmp(ent->d_name, ".", 1)) {
      printf("hidden\n");
      continue;
    }
    if (ent->d_type == DT_DIR) {
      char tmp[1024] = {0};
      sprintf(tmp, "%s/", ent->d_name);
      g_dirs[g_ent_num++] = strdup(tmp);
    }
    else {
      g_dirs[g_ent_num++] = strdup(ent->d_name);
    }
  }
  closedir(dir);
  return 0;
} /* search_dir() */

/*
 * free g_dirs and rest g_ent_num
 */

void free_dir_array() {
  if (g_dirs) {
    for (int i = 0; i < g_ent_num; i++) {
      free(g_dirs[i]);
    }
    free(g_dirs);
    g_dirs = NULL;
    g_ent_num = 0;
  }
} /* free_dir_array() */

/*
 * Get the icon link for the file
 */

char* get_icon(char* dirname, char* filename) {
  char* icon_link = NULL;
  char tmp[1024];
  sprintf(tmp, "http-root-dir/htdocs%s/%s", dirname, filename);
  if (filename[strlen(filename) - 1] == '/') {
    return strdup("/icons/folder.gif");
  }
  char* type = get_type(tmp);
  if (strstr(type, "image")) {
    icon_link = strdup("/icons/image.gif");
  }
  else if (strstr(type, "text")) {
    icon_link = strdup("/icons/text.gif");
  }
  else if (strstr(type, "directory")) {
    icon_link = strdup("/icons/folder.gif");
  }
  else {
    icon_link = strdup("/icons/text.gif");
  }
  return icon_link;
} /* get_icon() */

/*
 * Get the parent of current dir
 */

char* get_parent_dir(char* dirname) {
  if ( !strcmp(dirname, "/directory/")
      || !strcmp(dirname, "/directory")) {
    return strdup("/index.html");
  }
  else {
    char* tmp = dirname;
    if (tmp[strlen(tmp) - 1] == '/') {
      tmp[strlen(tmp) - 1] = '\0';
    }
    int len = strrchr(tmp, '/') - tmp + 1;
    printf("len: %d\n", len);
    char* parent = malloc(sizeof(char) * len + 1);
    strncpy(parent, tmp, len);
    parent[len] = '\0';
    return parent;
  }
} /* get_parent_dir() */

/*
 * Replace the %20 in uri with space
 */

void parse_space(char* string) {
  if (strstr(string, "%20") == NULL) {
    return;
  }
  int i = 0;
  for (i = 0; i < strlen(string); i++ ) {
    if (string[i] == '%') {
      break;
    }
  }
  string[i] = ' ';
  string[i + 1] = '\0';
  strcat(string, string + 3 + i);
} /* parse_space() */

/*
 * handle the directory browsing
 */

int handle_browsing(http_request* request, socket_t* sock) {
  g_dirs = (char**) malloc(g_max_num * sizeof(char*));
  parse_space(request->request_uri);
  int ret = search_dir(request->request_uri);
  if (ret) {
    return 1;
  }
  qsort(g_dirs, g_ent_num, sizeof(char*), str_cmp);
  for (int i = 0; i < g_ent_num; i++) {
    printf("%s ", g_dirs[i]);
  }
  printf("\n");
  socket_write_string(sock, "HTTP/1.1 200 OK\r\n");
  socket_write_string(sock, "Content-Type: text/html\r\n\r\n");

  socket_write_string(sock, "<html>\n");
  char html[1024];
  sprintf(html, "<head>\n<title>Index of %s</title></head>\n",
              request->request_uri);
  socket_write_string(sock, "<body>\n");
  sprintf(html, "<h1>Index of %s</h1>\n", request->request_uri);
  socket_write_string(sock, html);
  socket_write_string(sock, "<div>\n");
  socket_write_string(sock, "<tr>\n");
  socket_write_string(sock, "<td valign=\"top\">\n");
  socket_write_string(sock, "<img src=\"/icons/back.gif\">\n");
  socket_write_string(sock, "</td>\n");
  socket_write_string(sock, "<td>\n");
  char* tmp = get_parent_dir(request->request_uri);
  sprintf(html, "<a href=\"%s\">Parent Directory</a>", tmp);
  socket_write_string(sock, html);
  socket_write_string(sock, "</td>\n");
  socket_write_string(sock, "</tr>\n");
  socket_write_string(sock, "</div>\n");

  for (int i = 0; i < g_ent_num; i++) {
    socket_write_string(sock, "<div>\n");
    socket_write_string(sock, "<tr>\n");
    socket_write_string(sock, "<td valign=\"top\">\n");
    char* icon = get_icon(request->request_uri, g_dirs[i]);
    printf("icon: %s\n", icon);
    sprintf(html, "<img src=\"%s\">\n", icon);
    socket_write_string(sock, html);
    socket_write_string(sock, "</td>\n");
    socket_write_string(sock, "<td>\n");
    sprintf(html, "<a href=\"%s\">%s</a>", g_dirs[i], g_dirs[i]);
    socket_write_string(sock, html);
    socket_write_string(sock, "</td>\n");
    socket_write_string(sock, "</tr>\n");
    socket_write_string(sock, "</div>\n");
  }
  socket_write_string(sock, "</body>\n");
  socket_write_string(sock, "</html>\n");

  free_dir_array();
  return 0;
} /* handle_browsing() */
