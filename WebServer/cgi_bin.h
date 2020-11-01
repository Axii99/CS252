#include <stdio.h>
#include <stdlib.h>
#include "socket.h"

#include "http_messages.h"

void handle_cgi_bin(http_request* request, http_response* response, socket_t *sock);
