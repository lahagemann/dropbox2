#include "../include/dropboxClient.h"
#include "../include/dropboxUtil.h"
//#include "dropboxClient.h"
//#include "dropboxUtil.h"

client self;
char home[256];
const SSL_METHOD *sync_method;
const SSL_METHOD *main_method;
SSL_CTX *sync_context;
SSL_CTX *main_context;
SSL *ssl_sync;
SSL *ssl_main;

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

int authenticate_user(SSL *ssl, char *userid)
{
    printf("Authenticating user...\n");
    
    char buffer[BUFFER_SIZE];
    
    //manda userid para o server
    bzero(buffer, BUFFER_SIZE);
    memcpy(buffer, self.userid, MAXNAME);
    SSL_write(ssl, buffer, MAXNAME);

    // recebe um ok do servidor para continuar a conexão
    bzero(buffer, BUFFER_SIZE);
    SSL_read(ssl, buffer, 1);


    if(buffer[0] == 'A')
    {
        printf("Connected. :)\n");
        return 1;
    }
    else
    {
        printf("Authentication failed. Disconnecting...\n");
        return 0;
    }
}

//VIROU A FUNÇÃO DA THREAD SEPARADA DO DAEMON
void* sync_client(void *ssl)
{
    char buffer[BUFFER_SIZE];
    //int socketfd = *(int*)socket_sync;

    SSL *ssl_sync = (SSL *)ssl;

    // executa primeiro o sync server para não haver problemas
    while(1)
    {
        // faz isso a cada x segundos:
        sleep(SLEEP);

        printf("syncing...\n");
        
        // AQUI ETAPA DO SYNC_SERVER!
        update_self(&self, home, ssl_sync);
        
        // envia seu mirror pro servidor
        bzero(buffer, BUFFER_SIZE);
        memcpy(buffer, &self, sizeof(struct client));
        SSL_write(ssl_sync, buffer, sizeof(struct client));

        while(1)
        {
            char command;
            char fname[MAXNAME];

            bzero(buffer,BUFFER_SIZE);
            SSL_read(ssl_sync, buffer, 1);
            memcpy(&command, buffer, 1);
            if(command == DOWNLOAD)
            {
                // recebe nome do arquivo
                bzero(buffer,BUFFER_SIZE);
                SSL_read(ssl_sync, buffer, MAXNAME);
                memcpy(fname, buffer, MAXNAME);
                
                // procura arquivo
                int index = search_files(&self, fname);
                file_info f;
                if(index >= 0)
                    memcpy(&f, &self.fileinfo[index], sizeof(file_info));

                // manda struct
                bzero(buffer,BUFFER_SIZE);
                memcpy(buffer, &f, sizeof(file_info));
                SSL_write(ssl_sync, buffer, sizeof(file_info));
                
                // manda arquivo
                char fullpath[256];
                strcpy(fullpath, home);
                strcat(fullpath, "/sync_dir_");
                strcat(fullpath, self.userid);
                strcat(fullpath, "/");
                strcat(fullpath, f.name);
                strcat(fullpath, ".");
                strcat(fullpath, f.extension);

                // manda arquivo                
                send_file(fullpath, ssl_sync);
            }
            else if(command == DELETE)
            {
                // recebe nome do arquivo
                bzero(buffer,BUFFER_SIZE);
                SSL_read(ssl_sync, buffer, MAXNAME);
                memcpy(fname, buffer, MAXNAME);

                // procura arquivo
                int index = search_files(&self, fname);
                file_info f;

                if(index >= 0)
                    memcpy(&f, &self.fileinfo[index], sizeof(file_info));

                // deleta arquivo da pasta sync do server
                char fullpath[256];
                strcpy(fullpath, home);
                strcat(fullpath, "/sync_dir_");
                strcat(fullpath, self.userid);
                strcat(fullpath, "/");
                strcat(fullpath, f.name);
                strcat(fullpath, ".");
                strcat(fullpath, f.extension);
    
                remove_file(fullpath);

                // deleta estrutura da lista de arquivos do cliente
                delete_file_from_client_list(&self, fname);
            }
            else
                break;
        }
        // AGORA FAZ SYNC_CLIENT

        struct client server_mirror;
        struct file_info *fi;
    
        update_self(&self, home, ssl_sync);
    
        // envia para o servidor que ele vai começar o sync.
        bzero(buffer, BUFFER_SIZE);
        buffer[0] = SYNC;
        SSL_write(ssl_sync, buffer, 1);
        
        // envia para o servidor o seu login
        bzero(buffer,BUFFER_SIZE);
        memcpy(buffer, self.userid, MAXNAME);
        SSL_write(ssl_sync, buffer, MAXNAME);

          // fica esperando o servidor enviar sua estrutura deste client.
        int n = 0;
        bzero(buffer,BUFFER_SIZE);
        while(n < sizeof(struct client))
            n += SSL_read(ssl_sync, buffer+n, 1);
        memcpy(&server_mirror, buffer, sizeof(struct client));

          // pra cada arquivo do servidor:
          int i;
          for(i = 0; i < MAXFILES; i++) 
        {
              if(strcmp(server_mirror.fileinfo[i].name, "\0") == 0)
               break;
            else
            {
                // arquivo existe no cliente?
                int index = search_files(&self, server_mirror.fileinfo[i].name);

                if(index >= 0)        // arquivo existe no cliente
                {
                    //verifica se o arquivo no servidor é mais atual que o arquivo no cliente.
                    if(server_mirror.fileinfo[i].commit_modified > self.fileinfo[index].commit_modified)
                    {
                        //isso quer dizer que o arquivo no servidor é de um commit mais novo que o estado atual do cliente.
                        // pede para o servidor mandar o arquivo
                        bzero(buffer, BUFFER_SIZE);
                        buffer[0] = DOWNLOAD;
                        SSL_write(ssl_sync, buffer, 1);

                        bzero(buffer,BUFFER_SIZE);
                        memcpy(buffer, server_mirror.fileinfo[i].name, MAXNAME);
                        SSL_write(ssl_sync, buffer, MAXNAME);
                    
                        struct file_info f;
                        //fica esperando receber struct
                        bzero(buffer,BUFFER_SIZE);
                        int n = 0;
                        while(n < sizeof(struct file_info)){
                            n += SSL_read(ssl_sync, buffer+n, 1    );}
                        memcpy(&f, buffer, sizeof(struct file_info));

                        // receive file funciona com full path
                        char fullpath[256];
                        strcpy(fullpath, home);
                        strcat(fullpath, "/sync_dir_");
                        strcat(fullpath, self.userid);
                        strcat(fullpath, "/");
                        strcat(fullpath, f.name);
                        strcat(fullpath, ".");
                        strcat(fullpath, f.extension);

                        //recebe arquivo
                        receive_file(fullpath, ssl_sync);
                        // atualiza na estrutura do cliente.
                        self.fileinfo[index] = f;
                    }
                }
                else                // arquivo não existe no cliente
                {
                    if(self.current_commit == (server_mirror.current_commit - 1))
                    {
                        // o arquivo é velho e deve ser deletado do servidor adequadamente.
                        bzero(buffer, BUFFER_SIZE);
                        buffer[0] = DELETE;
                        SSL_write(ssl_sync, buffer, 1);

                        bzero(buffer,BUFFER_SIZE);
                        memcpy(buffer, server_mirror.fileinfo[i].name, MAXNAME);
                        SSL_write(ssl_sync, buffer, MAXNAME);
                    }
                    else
                    {
                        //isso quer dizer que é um arquivo novo colocado no servidor em outro pc.
                        // pede para o servidor mandar o arquivo
                        bzero(buffer, BUFFER_SIZE);
                        buffer[0] = DOWNLOAD;
                        SSL_write(ssl_sync, buffer, 1);

                        bzero(buffer,BUFFER_SIZE);
                        memcpy(buffer, server_mirror.fileinfo[i].name, MAXNAME);
                        SSL_write(ssl_sync, buffer, MAXNAME);

                        struct file_info f;
                        //fica esperando receber struct
                        bzero(buffer,BUFFER_SIZE);
                        int n = 0;
                        bzero(buffer,BUFFER_SIZE);
                        while(n < sizeof(struct file_info))
                            n += SSL_read(ssl_sync, buffer+n, 1);
                        memcpy(&f, buffer, sizeof(struct file_info));
                
                        // receive file funciona com full path
                        char fullpath[256];
                        strcpy(fullpath, home);
                        strcat(fullpath, "/sync_dir_");
                        strcat(fullpath, self.userid);
                        strcat(fullpath, "/");
                        strcat(fullpath, f.name);
                        strcat(fullpath, ".");
                        strcat(fullpath, f.extension);
                
                        //recebe arquivo
                        receive_file(fullpath, ssl_sync);

                        //bota f na estrutura self
                        insert_file_into_client_list(&self, f);
                    }
                }
            }
        }

        // avisa que acabou o seu sync.
        bzero(buffer, BUFFER_SIZE);
        buffer[0] = SYNC_END;
        SSL_write(ssl_sync, buffer, 1);

        //avança o estado de commit do cliente para o mesmo do servidor, já que ele atualizou.
        self.current_commit = server_mirror.current_commit; 

        printf("ending sync.\n");
    }
}

void create_SSL_method_contexts()
{
    // cria contextos para o SSL da thread principal do cliente
    main_method = SSLv23_client_method();
    main_context = SSL_CTX_new(main_method);
    if(main_context == NULL)
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    
    // cria contextos para o SSL da thread de sync
    sync_method = SSLv23_client_method();
    sync_context = SSL_CTX_new(sync_method);
    if(sync_context == NULL)
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
}

void add_SSL_to_main_socket(int socketfd)
{
    ssl_main = SSL_new(main_context);
    SSL_set_fd(ssl_main, socketfd);
    if(SSL_connect(ssl_main) == -1)
        ERR_print_errors_fp(stderr);
    else
    {
        X509 *certificate;
        char *line;
        certificate = SSL_get_peer_certificate(ssl_main);
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

void add_SSL_to_sync_socket(int socketfd)
{
    ssl_sync = SSL_new(sync_context);
    SSL_set_fd(ssl_sync, socketfd);
    if(SSL_connect(ssl_sync) == -1)
        ERR_print_errors_fp(stderr);
    else
    {
        X509 *certificate;
        char *line;
        certificate = SSL_get_peer_certificate(ssl_sync);
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

int main(int argc, char *argv[])
{
    int socketfd;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE];
    int sync_socketfd;

    if (argc <= MIN_ARG) 
    {
        fprintf(stderr,"usage %s hostname\n", argv[0]);
        exit(0);
    }

    strcpy(home,"/home/"); //home
    //strcpy(home,"/home/grad/");    //ufrgs
    strcat(home, getlogin());
    
    init_client(&self, home, argv[1]);
    init_SSL();
    create_SSL_method_contexts();
    
    // conecta este cliente com o servidor, que criará uma thread para administrá-lo
    socketfd = connect_server(argv[2], atoi(argv[3]));
    
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

    // recebe quantos clientes estão conectados para saber em que +x porta deve conectar
    bzero(buffer, BUFFER_SIZE);
    SSL_read(ssl_main, buffer, 1);
    int clients = buffer[0];

    // conecta um novo socket na porta +1 para fazer o sync apenas sem bloquear o programa de comandos.
    sync_socketfd = connect_server(argv[2], atoi(argv[3])+clients);

    add_SSL_to_sync_socket(sync_socketfd);

    // dispara nova thread pra fazer o sync_client

    int *newsync = malloc(1);
    *newsync = sync_socketfd;

    //inicializa mutex da fila de clientes
    if (pthread_mutex_init(&self.mutex, NULL) != 0)
    {
        printf("\nMutex (queue) init failed\n");
        return 0;
    }

    pthread_t initial_sync_client;
    pthread_create(&initial_sync_client, NULL, sync_client, (void*)ssl_sync);
    pthread_detach(initial_sync_client);
    
    bzero(buffer, BUFFER_SIZE);
    memcpy(buffer, self.userid, MAXNAME);
    SSL_write(ssl_main, buffer, MAXNAME);
    
    while(1) 
    {
        // AQUI: não sei se isso funciona, mas tem que separar o update do init.
        update_self(&self, home, ssl_main);
        
        bzero(buffer, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE, stdin);

        strcpy(message, buffer);

        char *msg = strtok(message, "\n");

        char *command = strtok(msg, " ");
        char *filepath = strtok(NULL, " ");

        if(strcmp(command, "list") == 0)
        {
            bzero(buffer, BUFFER_SIZE);
            buffer[0] = LIST;
            SSL_write(ssl_main, buffer, 1);

            // AQUI DÁ PROBLEMA, TEM QUE LER TODO O BUFFER
            int n = 0;
            bzero(buffer, BUFFER_SIZE);
            while(n < sizeof(struct client))
                n += SSL_read(ssl_main, buffer+n, 1);

            printf("%s", buffer);
        }
        else if(strcmp(command, "exit") == 0)
        {
            bzero(buffer, BUFFER_SIZE);
            buffer[0] = EXIT;
            SSL_write(ssl_main, buffer, 1);
            
            // Encerra os sockets
              SSL_shutdown(ssl_main);
              close(socketfd);
              SSL_free(ssl_main);

              SSL_shutdown(ssl_sync);
              close(sync_socketfd);
            SSL_free(ssl_sync);
            exit(0);
        }
        else if(strcmp(command, "upload") == 0)
        {
            bzero(buffer, BUFFER_SIZE);
            buffer[0] = UPLOAD;
            SSL_write(ssl_main, buffer, 1);

            bzero(buffer, BUFFER_SIZE);
            memcpy(buffer, filepath, 256);
            SSL_write(ssl_main, buffer, 256);
        
            // envia o arquivo;
            send_file(filepath, ssl_main);

            printf("File uploaded.\n");
        }
        else if(strcmp(command, "download") == 0)
        {
            bzero(buffer, BUFFER_SIZE);
            buffer[0] = DOWNLOAD;
            SSL_write(ssl_main, buffer, 1);

            // enviar o nome do arquivo
            // separar com strtok a extensão do arquivo

            char copy[256];
            strcpy(copy, filepath);

            char *name = strtok(copy, ".");

            bzero(buffer, BUFFER_SIZE);
            memcpy(buffer, name, MAXNAME);
            SSL_write(ssl_main, buffer, MAXNAME);

            // espera mensagem de ok do servidor:
            bzero(buffer, BUFFER_SIZE);
            SSL_read(ssl_main, buffer, 1);

            if(buffer[0] == FILE_FOUND)
            {
                // pega o diretório Downloads do usuário e baixa para lá.
                char user_downloads_dir[256];
                strcpy(user_downloads_dir, "/home/");
                //strcpy(user_downloads_dir, "/home/grad/");
                strcat(user_downloads_dir, getlogin());
                strcat(user_downloads_dir, "/Downloads/");
                strcat(user_downloads_dir, filepath);            

                // espera o arquivo
                receive_file(user_downloads_dir, ssl_main);

                printf("Downloaded file to /Downloads.\n");
            }
            else
            {
                printf("The file you asked for doesn't exit. Please try again.\n");
            }
        }
    }

    // Encerra os sockets
      SSL_shutdown(ssl_main);
      close(socketfd);
      SSL_free(ssl_main);

      SSL_shutdown(ssl_sync);
      close(sync_socketfd);
    SSL_free(ssl_sync);

    return 0;
}
