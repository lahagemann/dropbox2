//#include "fila2.h"
#include <dirent.h> 
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

// IF MAXNAME == 256, BUFFER_SIZE SHOULD BE 32KB
#define BUFFER_SIZE 16384
#define MAXNAME 128
#define MAXFILES 30
#define SLEEP 20
#define MAXCLIENTS 30

//flags de controle
#define DELETE 'x'
#define DOWNLOAD 'd'
#define EXIT 'e'
#define LIST 'l'
#define SYNC 's'
#define SYNC_END 'q'
#define UPLOAD 'u'
#define REC_TIME 't'
#define END_UPDATE 'w'

#define FILE_NOT_FOUND 'k'
#define FILE_FOUND 'y'

//connection flags
#define NOT_VALID 0
#define ACCEPTED 1


typedef struct file_info {
    char name[MAXNAME];
    char extension[MAXNAME];
    struct tm last_modified;
    int size;
    int commit_modified;
} file_info;

typedef struct client {
    int devices[2];
    char userid[MAXNAME];
    struct file_info fileinfo[MAXFILES];
    int logged_in;
    int current_commit;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} client;

typedef struct connection_info {
    int port;
    int socket_client;
    int socket_sync; 
    SSL *ssl_client;
    SSL *ssl_sync;
} connection_info;    

void init_client(client *client, char *home, char *login);
void init_self(client *client, char *home, char *login, SSL *ssl);
void update_client(client *client, char *home);
void update_self(client *client, char *home, SSL *ssl);
int search_files(client *client, char filename[MAXNAME]);
void insert_file_into_client_list(client *client, file_info fileinfo);
void delete_file_from_client_list(client *client, char filename[MAXNAME]);
//int file_more_recent_than(file_info f1, file_info f2);
void receive_file(char* file_name, SSL *ssl);
void send_file(char *file, SSL *ssl);
void remove_file(char *filename);
void init_SSL();

void christian(struct tm T0, struct tm T1, struct tm Ts, struct tm *Tc);
void diff_time(struct tm T1, struct tm T2, struct tm *Tret);
int more_recent(struct tm T1, struct tm T2);
