#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>

#include "socket.h"
#include "http_messages.h"

#define USE_SPECIFIED (1)
#define MAX_USR_PWD_LEN (60)

void run_linear_server(acceptor *acceptor);
void run_forking_server(acceptor *acceptor);
void run_threaded_server(acceptor *acceptor);
void run_thread_pool_server(acceptor *acceptor, int num_threads);
int parse_request(http_request *request, socket_t *sock);
int respond(http_request*, http_response*);
void init_start_time();
void init_logs();
void handle(socket_t *sock);

// Global variable for user and password
extern char g_user_pass[MAX_USR_PWD_LEN];

#endif // SERVER_H
