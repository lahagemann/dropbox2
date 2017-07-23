#include "../include/dropboxUtil.h"

char path[512];
const SSL_METHOD *backup_method;
SSL_CTX *backup_context;
SSL *ssl_backup;

void add_SSL_to_backup_socket(int socketfd)
{
    ssl_backup = SSL_new(backup_context);
    SSL_set_fd(ssl_backup, socketfd);
    if(SSL_connect(ssl_backup) == -1)
        ERR_print_errors_fp(stderr);
    else
    {
        X509 *certificate;
        char *line;
        certificate = SSL_get_peer_certificate(ssl_backup);
        if(certificate != NULL) 
        {
            line = X509_NAME_oneline(X509_get_subject_name(certificate),0,0);
            printf("Client: %s\n", line);
            free(line);
            line = X509_NAME_oneline(X509_get_issuer_name(certificate),0,0);
            printf("Certificate issuer: %s\n", line);
        }
    }
}

int connect_server(char *host, int port)
{
    int socketfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    if ( (server = gethostbyname(host)) == NULL ) 
    {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        printf("ERROR opening socket\n");
    
    serv_addr.sin_family = AF_INET;     
    serv_addr.sin_port = htons(port);    
    serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
    bzero(&(serv_addr.sin_zero), 8);     
     
    if (connect(socketfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        printf("ERROR connecting\n");

    return socketfd;
}

int main(int argc, char *argv[])
{
	int socketfd;
    char buffer[BUFFER_SIZE];

	if (argc <= 2) 
    {
        printf("invalid ip/port");
        exit(0);
    }

	strcpy(path,"/home/backup/"); //home
    //strcpy(home,"/home/grad/backup/"); //ufrgs

    init_SSL();
    
    // cria contextos para o SSL da thread principal do backup
    backup_method = SSLv23_client_method();
    backup_context = SSL_CTX_new(backup_method);
    if(backup_context == NULL)
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    
    // conecta este cliente com o servidor, que criará uma thread para administrá-lo
    socketfd = connect_server(argv[1], atoi(argv[2]));
    
    // adiciona SSL ao socket conectado.
    add_SSL_to_backup_socket(socketfd);
    
    // main loop do backup
    

	return 0;
}
