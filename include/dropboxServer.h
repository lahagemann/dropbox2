#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#define MIN_ARG 1
#define MAX_CONNECTIONS 5

void sync_server();
void* run_sync(void *socket_sync);
void* run_client(void *conn_info);
