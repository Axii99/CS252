#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#include "http_messages.h"
#include "htdocs.h"
#include "cgi_bin.h"
#include "misc.h"


#define MAX_REQ_LEN (4096)

char g_user_pass[MAX_USR_PWD_LEN];
sem_t g_pool;
pthread_mutex_t g_pool_mutex;
double g_max_time = 0;
double g_min_time = 0;
struct timespec start_time = {0};
int g_num_req = 0;

/*
 * initialize the start time
 */

void init_start_time() {
  clock_gettime(CLOCK_REALTIME, &start_time);
  FILE *fp = fopen("./http-root-dir/myhttpd.stat", "w+");
  char stat[2048];
  sprintf(stat, "Name: Yichao Ma\nServer Runtime: %ds\n"
                "Request Number: %d\n"
                "Min Service Time: %fs\n"
                "Max service Time: %fs\n",
                0, g_num_req, g_min_time, g_max_time);
  fwrite(stat, sizeof(char), strlen(stat), fp);
  fclose(fp);
} /*ubut_start_time() */

void init_logs() {
  FILE *fp = fopen("./http-root-dir/myhttpd.log", "w");
  fclose(fp);
}

/*
 * Return a string in a format <user>:<password> 
 * either from auth.txt or from your implememtation.
 */

char *return_user_pwd_string(void) {
  // Read from ./auth.txt. Don't change this. We will use it for testing
  FILE *fp = NULL;
  //char *line = (char *)malloc(sizeof(char) * MAX_USR_PWD_LEN);
  char *line = NULL;
  size_t len = 0;

  fp = fopen("./auth.txt", "r");
  if (fp == NULL) {
    perror("couldn't read auth.txt");
    exit(-1);
  }

  if (getline(&line, &len, fp) == -1) {
    perror("couldn't read auth.txt");
    free(line);
    line = NULL;
    exit(-1);
  }

  sprintf(g_user_pass, "%s", line);

  free(line);
  line = NULL;
  fclose(fp);
  return g_user_pass;
} /* return_user_pwd_string() */

/*
 * Accept connections one at a time and handle them.
 */

void run_linear_server(acceptor *acceptor) {
  while (1) {
    socket_t *sock = accept_connection(acceptor);
    handle(sock);
  }
} /* run_linear_server() */


/*
 * Accept connections, creating a different child process to handle each one.
 */

void run_forking_server(acceptor *acceptor) {
  // TODO: Add your code to accept and handle connections in child processes
  while (1) {
    socket_t *sock = accept_connection(acceptor);
    int ret = fork();
    if (ret == 0) {
      handle(sock);
      exit(0);
    }
    close_socket(sock);
  }
} /* run_forking_server() */

/*
 * Accept connections, creating a new thread to handle each one.
 */

void run_threaded_server(acceptor *acceptor) {
  // TODO: Add your code to accept and handle connections in new threads
  while (1) {
    socket_t *sock = accept_connection(acceptor);
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread, &attr, (void *(*)(void*))handle, (void*)sock);
  }
} /* run_threaded_server() */

void *loop_thread(acceptor *acceptor) {
  while (1) {
    //sem_wait(&g_pool);
    pthread_mutex_lock(&g_pool_mutex);
    socket_t *sock = accept_connection(acceptor);
    pthread_mutex_unlock(&g_pool_mutex);
    //sem_post(&g_pool);
    handle(sock);
  }
}

/*
 * Accept connections, drawing from a thread pool with num_threads to handle the
 * connections.
 */

void run_thread_pool_server(acceptor *acceptor, int num_threads) {
  // TODO: Add your code to accept and handle connections in threads from a
  // thread pool
  sem_init(&g_pool, 0, num_threads);
  pthread_mutex_init(&g_pool_mutex, NULL);
  pthread_t thread[num_threads];
  for (int i = 0; i < num_threads; i++) {
    pthread_create(&thread[i], NULL, (void *(*)(void*))loop_thread, acceptor);
  }
  for (int i = 0; i < num_threads; i++) {
    pthread_join(thread[i], NULL);
  }
  loop_thread(acceptor);
  pthread_mutex_destroy(&g_pool_mutex);
} /* run_thread_pool_server() */

/*
 * parse the request
 */

int parse_request(http_request *request, socket_t *sock) {
  int len = 0;
  char buf[MAX_REQ_LEN + 1] = {0};
  len = socket_read(sock, buf, MAX_REQ_LEN);
  g_num_req++;
  request->request_uri = "/";
  request->query = "";
  request->http_version = "HTTP/1.1";
  request->num_headers = 0;
  request->message_body = "";
  int i = 0;
  if (strncmp("GET", buf, 3)) {
    return 405;
  }
  else {
    request->method = "GET";
  }
  for (i = 4; i < len; i++) {
    if (buf[i] == ' ') {
      char url[i - 3];
      strncpy(url, &buf[4], i-4);
      url[i - 4] = '\0';
      request->request_uri = malloc(sizeof(url));
      strcpy(request->request_uri, url);
      break;
    }
  }
  
  int temp = i + 1;
  for (i = i + 1; i < len; i++) {
    if (buf[i] == '\r') {
      char version[i - temp + 1];
      strncpy(version, &buf[temp], i - temp);
      version[i - temp] = '\0';
      request->http_version = malloc(sizeof(version));
      strcpy(request->http_version, version);
      if (strncmp("HTTP", version, 4)) {
        return 400;
      }
      else if (strncmp("HTTP/1.1", version, 8)) {
        return 505;
      }
      break;
    }
  }
  int num = 0;
  temp = i + 2;
  if (temp >= len) {
      perror("malform");
      return 400;
  }
  for (i = i + 2; i < len - 1; i++) {
    if ((buf[i] == '\r') && (buf[i+1] == '\n')) {
      num++;
    }
  }
  num--;
  request->num_headers = num;
  if (num == 0) {
    return 0;
  }
  else if (num < 0){
    return 400;
  }
  request->headers = malloc(sizeof(header) * num);
  i = temp;
  for (int j = 0; j < num; j++) {
    if (temp == len) {
      perror("malform");
      return 400;
    }
    while (buf[temp] != ':') {
      temp++;
      if (temp == len) {
        perror("malform");
        return 400;
      }
    }
    char key[temp - i + 1];
    strncpy(key, &buf[i], temp - i);
    key[temp - i] = '\0';
    request->headers[j].key = malloc(sizeof(key));
    strcpy(request->headers[j].key, key);
    i = temp + 2;
    temp = i;
    if (temp == len) {
      perror("malform");
      return 400;
    }
    while (buf[temp] != '\r') {
      temp++;
      if (temp == len) {
        perror("malform");
        return 400;
      }
    }
    char value[temp - i + 1];
    strncpy(value, &buf[i], temp - i);
    value[temp - i] = '\0';
    request->headers[j].value = malloc(sizeof(value));
    strcpy(request->headers[j].value, value);
    temp = temp + 2;
    i = temp;
  }
  return 0;
} /* parse_request() */

/*
 * respond the request and check authorization
 */

int respond(http_request *request, http_response *response) {
  header *auth_header = NULL;
  const char* auth_char = "Authorization";
  for (int i = 0; i < request->num_headers; i++) {
    if (!strcmp(request->headers[i].key, auth_char)) {
      auth_header = &request->headers[i];
      break;
    }
  }
  if (auth_header == NULL) {
    response->status_code = 401;
    response->num_headers = 1;
    response->headers = malloc(sizeof(header));
    char* auth_key = "WWW-Authenticate\0";
    response->headers[0].key = malloc(strlen(auth_key) + 1);
    strcpy(response->headers[0].key, auth_key);
    char* auth_value = "Basic realm=\"CS252-Server\"\0";
    response->headers[0].value = malloc(strlen(auth_value) + 1);
    strcpy(response->headers[0].value, auth_value);
  }
  else {
    char* raw_code = auth_header->value;
    strcpy(raw_code, &(auth_header->value[6]));
    int output_length = 0;
    build_decoding_table();
    char* base64_code = (char*) base64_decode((const char*)raw_code,
                (size_t) strlen(raw_code), (size_t*) &output_length);
    base64_cleanup();
    char* pwd = g_user_pass;
    pwd[strlen(pwd) - 1] = '\0';
    if (!strcmp(pwd, base64_code)) {
      if (strstr(request->request_uri, "/cgi-bin/")) {
        return 1;
      }
      else {
        handle_htdocs(request, response);
      }
    }
    else {
      response->status_code = 401;
      response->num_headers = 1;
      response->headers = malloc(sizeof(header));
      char* auth_key = "WWW-Authenticate";
      response->headers[0].key = malloc(strlen(auth_key) * sizeof(char*) + 1);
      strcpy(response->headers[0].key, auth_key);
      char* auth_value = "Basic realm=\"CS252-Server\"\0";
      response->headers[0].value = malloc(sizeof(char*) *
                                  strlen(auth_value) + 1);
      strcpy(response->headers[0].value, auth_value);
    }
  }
  return 0;
} /*respond() */

/*
 * Update the statistics
 */

void update_stat(double total_time, double service_time) {
  FILE *fp = fopen("./http-root-dir/myhttpd.stat", "w+");
  if ((g_max_time == 0) && (g_min_time == 0)) {
    g_max_time = service_time;
    g_min_time = service_time;
  }
  if (g_max_time < service_time) {
    g_max_time = service_time;
  }
  if (g_min_time > service_time) {
    g_min_time = service_time;
  }
  char stat[2048];
  sprintf(stat, "Name: Yichao Ma\nServer Runtime: %ds\n"
                "Request Number: %d\n"
                "Min Service Time: %fs\n"
                "Max service Time: %fs\n",
                (int) total_time, g_num_req, g_min_time, g_max_time);
  fwrite(stat, sizeof(char), strlen(stat), fp);
  fclose(fp);
} /* update_stat() */

/*
 * Update the log
 */

void update_log(http_request *request, http_response *response) {
  FILE *fp = fopen("./http-root-dir/myhttpd.log", "a+");
  char log[4096];
  char host[128];
  for (int i = 0; i < request->num_headers; i++) {
    if (!strcmp("Host", request->headers[i].key)) {
      strcpy(host, request->headers[i].value);
    }
  }
  sprintf(log, "Host: %s\nURL: %s\nResponse:%s\n",
          host, request->request_uri, response_string(response));
  fwrite(log, sizeof(char), strlen(log), fp);
  fclose(fp);
} /* update_log() */

/*
 * Handle an incoming connection on the passed socket.
 */

void handle(socket_t *sock) {
  return_user_pwd_string();
  http_request request = {0};

  // TODO: Replace this code and actually parse the HTTP request

  clock_t start = 0;
  clock_t end = 0;
  start = clock();

  int check = parse_request(&request, sock);
  print_request(&request);
  http_response response = { 0 };
  // TODO: Add your code to create the correct HTTP response
  parse_space(request.request_uri);
  response.http_version = "HTTP/1.1";
  char temp[1024];
  sprintf(temp, "./http-root-dir/htdocs%s", request.request_uri);
  char* type = get_content_type(temp);
  if (check) {
    response.status_code = check;
    char *to_string = response_string(&response);
    socket_write_string(sock, to_string);
    free(to_string);
    to_string = NULL;
  }
  else if (strstr(request.request_uri, "directory")
          && (type != NULL)
          && (strstr(type, "inode/directory"))) {
    int ret = handle_browsing(&request, sock);
    if (ret) {
      char* header = "HTTP/1.1 404 NOT FOUND\r\nServer:CS252\r\n";
      socket_write_string(sock, header);
      socket_write_string(sock, "Content-Type: text/plain\r\n\r\n");
      socket_write_string(sock, "404 NOT FOUND\n");
    }
  }
  else if (!respond(&request, &response)){
    char *to_string = response_string(&response);
    int length = 0;
    for (int i = 0; i < response.num_headers; i++) {
      if  (!strcmp(response.headers[i].key, "Content-Length")) {
        length = atoi(response.headers[i].value);
        break;
      }
    }

    socket_write_string(sock, to_string);
    socket_write(sock, response.message_body, length);
    printf("%s\n", to_string);
    free(to_string);
    to_string = NULL;
  }
  else {
    char* header = "HTTP/1.1 200 Document follows\r\nServer:CS252\r\n";
    socket_write_string(sock, header);
    handle_cgi_bin(&request, &response, sock);
  }

  end = clock();
  double service_time = 0;
  double total_time = 0;
  service_time = ((double)end - start) / CLOCKS_PER_SEC;
  struct timespec curr_time = {0};
  clock_gettime(CLOCK_REALTIME, &curr_time);
  total_time = (double) (curr_time.tv_sec - start_time.tv_sec)
                        + (curr_time.tv_nsec - start_time.tv_nsec) / 1000000000;
  update_stat(total_time, service_time);
  update_log(&request, &response);
  close_socket(sock);
} /* handle() */
