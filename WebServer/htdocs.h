#include <stdio.h>
#include <stdlib.h>

#include "http_messages.h"
#include "socket.h"
int handle_htdocs(http_request*, http_response*);
int handle_browsing(http_request*, socket_t* );
void parse_space(char*);
