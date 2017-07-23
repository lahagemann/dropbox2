#include "../include/dropboxServer.h"
#include "../include/dropboxUtil.h"

//#include "dropboxServer.h"
//#include "dropboxUtil.h"
//#include "../../include/fila2.h"
//#include "fila2.h"

pthread_mutex_t queue;
int clients;
client connected_clients[MAXCLIENTS];
char home[256];

enum { STATE_DEV1, STATE_DEV2 } state = STATE_DEV1;

void list_files(client *client, char *buffer){
    int i;
    
    strcpy(buffer, "files:\n");     
    for(i = 0; i < MAXFILES; i++)
    {
        if (strcmp(client->fileinfo[i].name,"\0") == 0)
            break;
        else
        {
            strcat(buffer, client->fileinfo[i].name);
            strcat(buffer, ".");
            strcat(buffer, client->fileinfo[i].extension);
            strcat(buffer, "\n");
        }
    }
}

//-1 se nao achou o cliente i>=0 com o cliente na estrutura se achou
int return_client(char* user_name, client *new_client){
    int i;    
    pthread_mutex_lock(&queue);

    for(i=0;i<MAXCLIENTS;i++){
        if(connected_clients[i].logged_in == 1){
            if(strcmp(user_name, connected_clients[i].userid) == 0){
                pthread_mutex_unlock(&queue);
                return i;
            }
        }
    }
    pthread_mutex_unlock(&queue);
    return -1;
}

void disconnect_client(client *clientinfo){
    int i;
    pthread_mutex_lock(&queue);

    
    for(i=0;i<MAXCLIENTS;i++){
        if(connected_clients[i].logged_in == 1){
            if(strcmp(clientinfo->userid, connected_clients[i].userid) == 0){
                break;
            }
        }
    }
        
    //se tem dois conectados, disconecta um
    if(connected_clients[i].devices[1] == 1)
        connected_clients[i].devices[1] = 0;
    else //se não, desloga
        connected_clients[i].logged_in = 0;
        
    pthread_mutex_unlock(&queue);

    clients -= 1;
}

int insert_client(client *clientinfo){
    pthread_mutex_lock(&queue);
    int i;

    for(i=0;i<MAXCLIENTS;i++){
        if(connected_clients[i].logged_in == 1)
        {
            if(strcmp(clientinfo->userid, connected_clients[i].userid) == 0)
            {
                if(connected_clients[i].devices[1] == 0)
                {
                    //connected_clients[i].devices[1] = 1; //ja esta conectado, ativar segundo device
                    pthread_mutex_unlock(&queue);
                    return ACCEPTED;
                }
                else
                {
                    pthread_mutex_unlock(&queue);
                    return NOT_VALID;
                }
            }
        }
        else
        {
            memcpy(&connected_clients[i], clientinfo, sizeof(client));
            //connected_clients[i] = *clientinfo;
            //connected_clients[i].devices[0] = 1;
            pthread_mutex_unlock(&queue);
            return ACCEPTED;
        }
    }

    pthread_mutex_unlock(&queue);

    return NOT_VALID;
}

void* run_client(void *ssl)
{
    char buffer[BUFFER_SIZE];
    char message;
    SSL *ssl_main = (SSL *)ssl; 

    char clientid[MAXNAME];
    
    bzero(buffer, BUFFER_SIZE);
    SSL_read(ssl_main, buffer, MAXNAME);
    memcpy(clientid, buffer, MAXNAME);
    
    client *cli = malloc(sizeof(client));
 
    while(1) {
        bzero(buffer, BUFFER_SIZE);
        message = SSL_read(ssl_main, buffer, 1);
        if (message < 0) 
            printf("ERROR reading from socket");
        else {
            memcpy(&message, buffer, 1);

            //nao sei se é exatamente assim
            switch(message){
                case EXIT:
                    {
                        disconnect_client(cli);
                        pthread_exit(0);
                    }                    
                    break;
                case DOWNLOAD:
                    {
                        bzero(buffer, BUFFER_SIZE);
                        SSL_read(ssl_main, buffer, MAXNAME);

                        int client_index = return_client(clientid, cli);                        

                        int index = search_files(&(connected_clients[client_index]), buffer);
                
                        if(index >= 0)
                        {
                            // manda mensagem de arquivo existente.
                            bzero(buffer, BUFFER_SIZE);
                            buffer[0] = FILE_FOUND;
                            SSL_write(ssl_main, buffer, 1);

                            // prepara o caminho do diretório sync para enviar o arquivo
                            char fullpath[MAXNAME];
                            strcpy(fullpath, home);
                            strcat(fullpath, "/sync_dir_");
                            strcat(fullpath, connected_clients[client_index].userid);
                            strcat(fullpath, "/");
                            strcat(fullpath, connected_clients[client_index].fileinfo[index].name);
                            strcat(fullpath, ".");
                            strcat(fullpath, connected_clients[client_index].fileinfo[index].extension);

                            // manda arquivo    
                            send_file(fullpath, ssl_main);
                        }
                        else
                        {
                            // manda mensagem de arquivo inexistente.
                            bzero(buffer, BUFFER_SIZE);
                            buffer[0] = FILE_NOT_FOUND;
                            SSL_write(ssl_main, buffer, 1);
                        }
                    }
                    break;
                case UPLOAD:
                    {
                        bzero(buffer, BUFFER_SIZE);
                        SSL_read(ssl_main, buffer, MAXNAME);

                        // pegar o último elemento
                        char file[256];
                        strcpy(file, buffer);
                        
                        char *filename = strrchr(file, '/');

                        // pegar path de destino do servidor
                        int client_index = return_client(clientid, cli);

                        char fullpath[MAXNAME];
                        strcpy(fullpath, home);
                        strcat(fullpath, "/sync_dir_");
                        strcat(fullpath, connected_clients[client_index].userid);
                        strcat(fullpath, filename);

                        receive_file(fullpath, ssl_main);
                    }                    
                    break;
                case LIST:
                    {
                        int client_index = return_client(clientid, cli);
                        
                        bzero(buffer, BUFFER_SIZE);
                        list_files(&(connected_clients[client_index]), buffer);
                        SSL_write(ssl_main, buffer, BUFFER_SIZE);
                    }
                case REC_TIME:
                    {
                        bzero(buffer, BUFFER_SIZE);
                        time_t server_time;
                        time(&server_time);
                        memcpy(&buffer, localtime(&server_time), sizeof(struct tm));
                        SSL_write(ssl_main, buffer, sizeof(struct tm));
                    }
                    break;
                default:
                    break;
            }
        }
    }

    SSL_shutdown(ssl_main);
    //    close(socketfd);
    SSL_free(ssl_main);
}

void* run_sync(void *conn_info)
{
    char buffer[BUFFER_SIZE];
    connection_info ci = *(connection_info *)conn_info;
    SSL *ssl_sync = ci.ssl_sync;
    int socketfd = ci.socket_sync;
    char message;

    int cliindex;
    
    while(1) 
    {
        printf("syncing...\n");
        // aí executa aqui o sync_server.
        sync_server(ci);

        // cliente está executando update_self. server ouve e envia tempo quando necessário.
        bzero(buffer, BUFFER_SIZE);
        SSL_read(ssl_sync, buffer, 1);
        while(buffer[0] != END_UPDATE)
        {
            if(buffer[0] == REC_TIME)
            {
                bzero(buffer, BUFFER_SIZE);
                time_t server_time;
                time(&server_time);
                memcpy(&buffer, localtime(&server_time), sizeof(struct tm));
                SSL_write(ssl_sync, buffer, sizeof(struct tm));
            }
            bzero(buffer, BUFFER_SIZE);
            SSL_read(ssl_sync, buffer, 1);
        }

        bzero(buffer, BUFFER_SIZE);
        message = SSL_read(ssl_sync, buffer, 1);

        if (message < 0) 
            printf("ERROR reading from socket");
        else 
        {
            message = buffer[0];

            if(message == SYNC)
            {
                char client_id[MAXNAME];
                //recebe id do cliente. ---> VER SE NÃO É MELHOR ELE RECEBER ANTES???
                //pegar os dados do buffer
                bzero(buffer, BUFFER_SIZE);
                SSL_read(ssl_sync, buffer, MAXNAME);
                memcpy(client_id, buffer, MAXNAME);

                // pega cliente na lista de clientes e envia o mirror para o cliente.
                client *cli = malloc(sizeof(client));
                cliindex = return_client(client_id, cli);

                update_client(&(connected_clients[cliindex]), home);
    
                // AQUI DÁ PROBLEMA
                bzero(buffer,BUFFER_SIZE);
                memcpy(buffer, &(connected_clients[cliindex]), sizeof(client));
                SSL_write(ssl_sync, buffer, sizeof(client));

                // agora fica em um while !finished, fica recebendo comandos de download/delete
                while(1)
                {    
                    char command;
                    char fname[MAXNAME];

                    bzero(buffer,BUFFER_SIZE);
                    SSL_read(ssl_sync, buffer, 1);
                    command = buffer[0];

                    if(command == DOWNLOAD)
                    {
                        // recebe nome do arquivo
                        bzero(buffer,BUFFER_SIZE);
                        SSL_read(ssl_sync, buffer, MAXNAME);
                        memcpy(fname, buffer, MAXNAME);
                        
                        // procura arquivo
                        int index = search_files(&(connected_clients[cliindex]), fname);
                        struct file_info f;

                        if(index >= 0)
                            memcpy(&f, &(connected_clients[cliindex].fileinfo[index]), sizeof(struct file_info));

                        // manda struct    
                        bzero(buffer,BUFFER_SIZE);
                        memcpy(buffer, &f, sizeof(struct file_info));
                        SSL_write(ssl_sync, buffer, sizeof(struct file_info));

                        // receive file funciona com full path
                        char fullpath[MAXNAME];
                        strcpy(fullpath, home);
                        strcat(fullpath, "/sync_dir_");
                        strcat(fullpath, connected_clients[cliindex].userid);
                        strcat(fullpath, "/");
                        strcat(fullpath, f.name);
                        strcat(fullpath, ".");
                        strcat(fullpath, f.extension);


                        // manda arquivo                
                        send_file(fullpath, ssl_sync);
                    }
                    else if(command == DELETE)
                    {
                        bzero(buffer,BUFFER_SIZE);

                        // recebe nome do arquivo
                        SSL_read(ssl_sync, buffer, MAXNAME);
                        memcpy(fname, buffer, MAXNAME);

                        // procura arquivo
                        int index = search_files(&(connected_clients[cliindex]), fname);
                        file_info f;

                        if(index >= 0)
                            memcpy(&f,&(connected_clients[cliindex].fileinfo[index]), sizeof(file_info));

                        // deleta arquivo da pasta sync do server
                        char fullpath[MAXNAME];
                        strcpy(fullpath, home);
                        strcat(fullpath, "/sync_dir_");
                        strcat(fullpath, connected_clients[cliindex].userid);
                        strcat(fullpath, "/");
                        strcat(fullpath, f.name);
                        strcat(fullpath, ".");
                        strcat(fullpath, f.extension);
            
                        remove_file(fullpath);

                        // deleta estrutura da lista de arquivos do cliente
                        delete_file_from_client_list(&(connected_clients[cliindex]), fname);
                    }
                    else
                    {
                        //pthread_mutex_unlock(&connected_clients[cliindex].mutex);
                        break;
                    }
                }
            }
        }
        
        pthread_mutex_lock(&connected_clients[cliindex].mutex);
        if(socketfd == connected_clients[cliindex].devices[0] && connected_clients[cliindex].devices[1] != 0)
            state = STATE_DEV2;
        else
            state = STATE_DEV1;
        pthread_cond_signal(&connected_clients[cliindex].cond);
        pthread_mutex_unlock(&connected_clients[cliindex].mutex);

        printf("ending sync.\n");
    }
}

void sync_server(connection_info ci)
{
    struct client client_mirror;
    char buffer[BUFFER_SIZE];
    
    SSL *ssl_sync = ci.ssl_sync;
    int socketfd = ci.socket_sync;
    
    bzero(buffer, BUFFER_SIZE);
    SSL_read(ssl_sync, buffer, 1);
    while(buffer[0] != END_UPDATE)
    {
        if(buffer[0] == REC_TIME)
        {
            bzero(buffer, BUFFER_SIZE);
            time_t server_time;
            time(&server_time);
            memcpy(&buffer, localtime(&server_time), sizeof(struct tm));
            SSL_write(ssl_sync, buffer, sizeof(struct tm));
        }
        bzero(buffer, BUFFER_SIZE);
        SSL_read(ssl_sync, buffer, 1);
    }

    //server fica esperando o cliente enviar seu mirror
    // AQUI DÁ PROBLEMA
    int n = 0;
    bzero(buffer,BUFFER_SIZE);
    while(n < sizeof(struct client))
        n += SSL_read(ssl_sync, buffer+n, 1);
    memcpy(&client_mirror, buffer, sizeof(struct client));
    

    client *cli = malloc(sizeof(client));
    int cliindex = return_client(client_mirror.userid, cli);
    
    //tem que fazer cond wait com relação ao device que está conectado.
    if(socketfd == connected_clients[cliindex].devices[0])
    {
        pthread_mutex_lock(&connected_clients[cliindex].mutex);
        while(state != STATE_DEV1)
            pthread_cond_wait(&connected_clients[cliindex].cond, &connected_clients[cliindex].mutex);
        pthread_mutex_unlock(&connected_clients[cliindex].mutex);
    }
    else
    {
        pthread_mutex_lock(&connected_clients[cliindex].mutex);
        while(state != STATE_DEV2)
            pthread_cond_wait(&connected_clients[cliindex].cond, &connected_clients[cliindex].mutex);
        pthread_mutex_unlock(&connected_clients[cliindex].mutex);
    }

    update_client(&(connected_clients[cliindex]), home);

      // pra cada arquivo do cliente:
      int i;
      for(i = 0; i < MAXFILES; i++) 
    {
          if(strcmp(client_mirror.fileinfo[i].name, "\0") == 0)
        {
               break;
        }
        else
        {
            // arquivo existe no servidor?
            int index = search_files(&(connected_clients[cliindex]), client_mirror.fileinfo[i].name);
            
            if(index >= 0)        // arquivo existe no servidor
            {
                //verifica se o arquivo no cliente é mais atual que o arquivo no servidor.
                if(client_mirror.fileinfo[i].commit_modified > connected_clients[cliindex].fileinfo[index].commit_modified)
                {
                    //isso quer dizer que o arquivo no cliente é mais atual que o arquivo deste cliente no servidor. deve ser baixado, portanto.
                    // pede para o cliente mandar o arquivo
                    bzero(buffer,BUFFER_SIZE);
                    buffer[0] = DOWNLOAD;
                    SSL_write(ssl_sync, buffer, 1);

                    bzero(buffer,BUFFER_SIZE);
                    memcpy(buffer, &client_mirror.fileinfo[i].name, MAXNAME);
                    SSL_write(ssl_sync, buffer, MAXNAME);

                    // DA PROBLEMA COM TAMANHO DO BUFFER:
                    struct file_info f;
                    //fica esperando receber struct
                    bzero(buffer,BUFFER_SIZE);
                    int n = 0;
                    bzero(buffer,BUFFER_SIZE);
                    while(n < sizeof(struct file_info))
                        n += SSL_read(ssl_sync, buffer+n, 1);
                    memcpy(&f, buffer, sizeof(struct file_info));

                
                    // receive file funciona com full path
                    char fullpath[MAXNAME];
                    strcpy(fullpath, home);
                    strcat(fullpath, "/sync_dir_");
                    strcat(fullpath, connected_clients[cliindex].userid);
                    strcat(fullpath, "/");
                    strcat(fullpath, f.name);
                    strcat(fullpath, ".");
                    strcat(fullpath, f.extension);

            
                    //recebe arquivo
                    receive_file(fullpath, ssl_sync);

                    // atualiza na estrutura do cliente no servidor.
                    memcpy(&(connected_clients[cliindex].fileinfo[index]), &f, sizeof(file_info));
                }
            }
            else                // arquivo não existe no servidor
            {
                int server_commit = connected_clients[cliindex].current_commit;

                // device 2 executando
                if(server_commit == (client_mirror.current_commit + 1))    
                    server_commit -= 1;

                // verifica se o arquivo no cliente tem um commit_modified > state do servidor
                if(client_mirror.current_commit == server_commit)
                {
                    //isso quer dizer que é um arquivo novo colocado no servidor em outro pc.
                    // pede para o cliente mandar o arquivo
                    bzero(buffer,BUFFER_SIZE);
                    buffer[0] = DOWNLOAD;
                    SSL_write(ssl_sync, buffer, 1);

                    bzero(buffer,BUFFER_SIZE);
                    memcpy(buffer, client_mirror.fileinfo[i].name, MAXNAME);
                    SSL_write(ssl_sync, buffer, MAXNAME);

                    struct file_info f;
                    //fica esperando receber struct
                    int n = 0;
                    bzero(buffer,BUFFER_SIZE);
                    while(n < sizeof(struct file_info))
                        n += SSL_read(ssl_sync, buffer+n, 1);
                    memcpy(&f, buffer, sizeof(struct file_info));

        
                    // receive file funciona com full path
                    char fullpath[MAXNAME];
                    strcpy(fullpath, home);
                    strcat(fullpath, "/sync_dir_");
                    strcat(fullpath, connected_clients[cliindex].userid);
                    strcat(fullpath, "/");
                    strcat(fullpath, f.name);
                    strcat(fullpath, ".");
                    strcat(fullpath, f.extension);

                    //recebe arquivo
                    receive_file(fullpath, ssl_sync);
            

                    //bota f na estrutura self
                    insert_file_into_client_list(&(connected_clients[cliindex]), f);
                }
                else
                {
                    // deleta arquivo no cliente.
                    // o arquivo é velho e deve ser deletado do servidor adequadamente.
                    bzero(buffer, BUFFER_SIZE);
                    buffer[0] = DELETE;
                    SSL_write(ssl_sync, buffer, 1);

                    bzero(buffer,BUFFER_SIZE);
                    memcpy(buffer, &connected_clients[cliindex].fileinfo[i].name, MAXNAME);
                    SSL_write(ssl_sync, buffer, MAXNAME);
                }
            }
        }
    }

    //avança o estado de commit do cliente no servidor.
    connected_clients[cliindex].current_commit += 1;

    // avisa que acabou o seu sync.
    bzero(buffer, BUFFER_SIZE);
    buffer[0] = SYNC_END;
    SSL_write(ssl_sync, buffer, 1);
}

int main(int argc, char *argv[])
{
    clients = 0;
    int i;
    char buffer[BUFFER_SIZE];

    strcpy(home,"/home/");    //home
    //strcpy(home,"/home/grad/");    //ufrgs
    strcat(home, getlogin());
    strcat(home, "/server");

    struct stat st;
    if (stat(home, &st) != 0) {
          mkdir(home, 0777);
    }
        
    for(i=0;i<MAXCLIENTS;i++){
        connected_clients[i].logged_in = 0;
    }   

    int socket_connection, socket_client;
    socklen_t client_len;
    struct sockaddr_in serv_addr, client_addr;


    //inicializa mutex da fila de clientes
    if (pthread_mutex_init(&queue, NULL) != 0)
    {
        printf("\nMutex (queue) init failed\n");
        return 0;
    }

    if(argc <= MIN_ARG)
    {
        printf("Not enough arguments passed.");
        exit(1);
    }

    // inicialização das estruturas necessarias para o SSL
    init_SSL();

    const SSL_METHOD *main_method;
    SSL_CTX *main_context;
    SSL *ssl_main;

    const SSL_METHOD *sync_method;
    SSL_CTX *sync_context;
    SSL *ssl_sync;
    
    const SLL_METHOD *backup_method;
    SSL_CTX *backup_context;
    SSL *ssl_backup;

    // inicialização dos contextos SSL da main thread e da sync thread.
    main_method = SSLv23_server_method();
    main_context = SSL_CTX_new(main_method);
    if (main_context == NULL) {
        ERR_print_errors_fp(stderr);
        abort();
    }

    sync_method = SSLv23_server_method();
    sync_context = SSL_CTX_new(sync_method);
    if(sync_context == NULL)
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    
    backup_method = SSLv23_server_method();
    backup_context = SSL_CTX_new(backup_method);
    if(backup_context == NULL)
    {
        ERR_print_errors_fp(stderr);
        abort();
    }

    // load dos certificados do SSL para as threads main, sync e backup
    SSL_CTX_use_certificate_file(main_context, "CertFile.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(main_context, "KeyFile.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_certificate_file(sync_context, "CertFile.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(sync_context, "KeyFile.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_certificate_file(backup_context, "CertFile.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(backup_context, "KeyFile.pem", SSL_FILETYPE_PEM);
    
    int PORT = atoi(argv[1]);
    
    if ((socket_connection = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        printf("ERROR opening socket");
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(serv_addr.sin_zero), 8);     
    
    if (bind(socket_connection, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        printf("ERROR on binding");
    
    listen(socket_connection, MAX_CONNECTIONS);
    client_len = sizeof(struct sockaddr_in);

	
	//socket backup
	int socket_backconn, socket_backup;
    socklen_t backup_len;
    struct sockaddr_in backup_addr;
    
    int PORT_BACKUP = PORT-1;

    if ((socket_backconn = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        printf("ERROR opening backup conn socket");

    backup_addr.sin_family = AF_INET;
    backup_addr.sin_port = htons(PORT_BACKUP);
    backup_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(backup_addr.sin_zero), 8);     

    if (bind(socket_backconn, (struct sockaddr *) &backup_addr, sizeof(backup_addr)) < 0) 
        printf("ERROR on binding backup");

    listen(socket_backconn, 1);
    backup_len = sizeof(struct sockaddr_in);

    if( (socket_backup = accept(socket_backconn, (struct sockaddr *) &backup_addr, &backup_len)) )
    {
        //inicializa o SSL para a thread de sync
        ssl_sync = SSL_new(backup_context);
        SSL_set_fd(ssl_backup, socket_backup);
        int ssl_err = SSL_accept(ssl_backup);
        if(ssl_err <= 0)
        {
            printf("[sync] Error initializing SSL\n");
            exit(1);
        }

        pthread_t backup_thread;
        // CRIAR A THREAD DE BACKUP
        pthread_create(&backup_thread, NULL, run_sync, (void*)ssl_backup);
        pthread_detach(backup_thread);
    }

    
    while(1)
    {
        if( (socket_client = accept(socket_connection, (struct sockaddr *) &client_addr, &client_len)) )

        {
            // SSL handshake
            ssl_main = SSL_new(main_context);
            SSL_set_fd(ssl_main, socket_client);
            int ssl_err = SSL_accept(ssl_main);
            if(ssl_err <= 0)
            {
                printf("[main] Error initializing SSL\n");
                exit(1);
            }

            char clientid[MAXNAME];

            // cliente me manda id
            bzero(buffer, BUFFER_SIZE);
            SSL_read(ssl_main, buffer, MAXNAME);
            memcpy(clientid, buffer, MAXNAME);


            client *cli = malloc(sizeof(client));

            init_client(cli, home, clientid);

            if(insert_client(cli) == ACCEPTED)
            {
                bzero(buffer, BUFFER_SIZE);
                //sprintf(buffer, "%s", "A");
                buffer[0] = 'A';        
                SSL_write(ssl_main, buffer, 1);
                clients += 1;
            }
            else
            {
                bzero(buffer, BUFFER_SIZE);
                buffer[0] = 'N';
                SSL_write(ssl_main, buffer, 1);
        
                close(socket_client);
                break;
            }
                
            pthread_t client_thread;
            pthread_create(&client_thread, NULL, run_client, (void*)ssl_main);
            pthread_detach(client_thread);

            // agora cria a outra conexão. 
            // envia quantos clientes estão conectados para o cliente saber em que +x porta deve conectar o sync
            bzero(buffer, BUFFER_SIZE);
            buffer[0] = clients;
            SSL_write(ssl_main, buffer, 1);

            // fica esperando a segunda conexão do sync e quando recebe, cria outro socket/thread.
            int socket_conn, socket_sync;
            socklen_t sync_len;
            struct sockaddr_in sync_addr;
            
            int PORT_SYNC = PORT+clients;
    
            if ((socket_conn = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
                printf("ERROR opening sync socket");
    
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(PORT_SYNC);
            serv_addr.sin_addr.s_addr = INADDR_ANY;
            bzero(&(serv_addr.sin_zero), 8);     
        
            if (bind(socket_conn, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
                printf("ERROR on binding sync");
    
            listen(socket_conn, 1);
            sync_len = sizeof(struct sockaddr_in);
    
            if( (socket_sync = accept(socket_conn, (struct sockaddr *) &sync_addr, &sync_len)) )
            {
                //inicializa o SSL para a thread de sync
                ssl_sync = SSL_new(sync_context);
                SSL_set_fd(ssl_sync, socket_sync);
                int ssl_err = SSL_accept(ssl_sync);
                if(ssl_err <= 0)
                {
                    printf("[sync] Error initializing SSL\n");
                    exit(1);
                }
                
                connection_info *ci = malloc(sizeof(connection_info));
                   ci->port = PORT_SYNC;
                   ci->socket_sync = socket_sync;
                   
                   ci->ssl_sync = malloc(sizeof(SSL));
                   ci->ssl_sync = ssl_sync;

                // atualiza o devices do cliente com o socketfd usado pra sync

                int in = return_client(clientid, cli);

                if(connected_clients[in].devices[0] == 0)
                    connected_clients[in].devices[0] = socket_sync;
                else
                    connected_clients[in].devices[1] = socket_sync;
 
                pthread_t sync_thread;
                pthread_create(&sync_thread, NULL, run_sync, (void*)ci);
                pthread_detach(sync_thread);
            }
        }
    }

    return 0; 
}
