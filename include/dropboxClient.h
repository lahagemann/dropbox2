#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <pthread.h>

#define MIN_ARG 3

//AEHOOOOOOOOOOOOO

int connect_server(char *host, int port);
void* sync_client(void *socket_sync);
void close_connection();
