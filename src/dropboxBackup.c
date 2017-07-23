#include "../include/dropboxUtil.h"

char path[512];

int main()
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
    create_SSL_method_contexts();
    
    // conecta backup com o servidor
    int socketfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

	char host[512];
	int port;

	strcpy(host, argv[1]);
	port = atoi(argv[2]);	

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

    
    // adiciona SSL ao socket conectado.
    add_SSL_to_main_socket(socketfd);
    
    // autentica usuário conectado
    int user_valid = authenticate_user(ssl_main, self.userid);
    
    if(!user_valid)
    {
        SSL_shutdown(ssl_sync);
        SSL_free(ssl_sync);
        close(socketfd);
        exit(1);
    }

	return 0;
}
