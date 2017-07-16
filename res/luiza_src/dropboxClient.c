//#include "../include/dropboxClient.h"
//#include "../include/dropboxUtil.h"
#include "dropboxClient.h"
#include "dropboxUtil.h"

client self;
char home[256];

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

//VIROU A FUNÇÃO DA THREAD SEPARADA DO DAEMON
void* sync_client(void *socket_sync)
{
	char buffer[BUFFER_SIZE];
	int socketfd = *(int*)socket_sync;

	// executa primeiro o sync server para não haver problemas

	while(1)
	{
		printf("syncing...\n");
		// faz isso a cada x segundos:
		sleep(SLEEP);
		
		// AQUI ETAPA DO SYNC_SERVER!
		printf("syncing server...\n");

		update_client(&self, home);

		// envia seu mirror pro servidor
		bzero(buffer, BUFFER_SIZE);
		memcpy(buffer, &self, sizeof(struct client));
		write(socketfd, buffer, BUFFER_SIZE);

		printf("sent struct self\n");

		while(1)
		{
			printf("receiving commands from server...\n");
			bzero(buffer,BUFFER_SIZE);

			char command;
			char fname[MAXNAME];

			read(socketfd, buffer, BUFFER_SIZE);
			memcpy(&command, buffer, 1);

			if(command == DOWNLOAD)
			{
				// recebe nome do arquivo
				bzero(buffer,BUFFER_SIZE);
				read(socketfd, buffer, BUFFER_SIZE);
				memcpy(fname, buffer, MAXNAME);
				
				// procura arquivo
				int index = search_files(&self, fname);
				file_info f;
				if(index >= 0)
					f = self.fileinfo[index];

				printf("sending file %s\n", f.name);

				// manda struct
				bzero(buffer,BUFFER_SIZE);
				memcpy(buffer, &f, sizeof(file_info));
				write(socketfd, buffer, BUFFER_SIZE);
				
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
				send_file(fullpath, socketfd);
			}
			else if(command == DELETE)
			{
				// recebe nome do arquivo
				bzero(buffer,BUFFER_SIZE);
				read(socketfd, buffer, BUFFER_SIZE);
				memcpy(fname, buffer, MAXNAME);

				// procura arquivo
				int index = search_files(&self, fname);
				file_info f;

				if(index >= 0)
					f = self.fileinfo[index];

				printf("deleting file %s ...\n", f.name);

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

		printf("syncing client...\n");

		struct client server_mirror;
		struct file_info *fi;
	
		update_client(&self, home);
	
		// envia para o servidor que ele vai começar o sync.
		bzero(buffer, BUFFER_SIZE);
		buffer[0] = SYNC;
		write(socketfd, buffer, BUFFER_SIZE);
		
		// envia para o servidor o seu login
		bzero(buffer,BUFFER_SIZE);
		memcpy(buffer, self.userid, MAXNAME);
		write(socketfd, buffer, BUFFER_SIZE);

	  	// fica esperando o servidor enviar sua estrutura deste client.
		bzero(buffer,BUFFER_SIZE);
		read(socketfd, buffer, BUFFER_SIZE);
		memcpy(&server_mirror, buffer, sizeof(struct client));

		printf("cli cc: %d x serv cc: %d\n", self.current_commit, server_mirror.current_commit);
	  
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

				if(index >= 0)		// arquivo existe no cliente
				{
					printf("found server file in client dir.\n");
					//verifica se o arquivo no servidor é mais atual que o arquivo no cliente.
					if(server_mirror.fileinfo[i].commit_modified > self.fileinfo[index].commit_modified)
					{
						printf("file in server is more recent than the one in client's dir.\n");
						//isso quer dizer que o arquivo no servidor é de um commit mais novo que o estado atual do cliente.
						// pede para o servidor mandar o arquivo
						bzero(buffer, BUFFER_SIZE);
						buffer[0] = DOWNLOAD;
						write(socketfd, buffer, BUFFER_SIZE);

						bzero(buffer,BUFFER_SIZE);
						memcpy(buffer, server_mirror.fileinfo[i].name, MAXNAME);
						write(socketfd, buffer, BUFFER_SIZE);
					
						struct file_info f;
						//fica esperando receber struct
						bzero(buffer,BUFFER_SIZE);
						read(socketfd, buffer, BUFFER_SIZE);
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
						receive_file(fullpath, socketfd);

						// atualiza na estrutura do cliente.
						self.fileinfo[index] = f;
					}
				}
				else				// arquivo não existe no cliente
				{
					printf("did not find file in client's dir.\n");
					if(self.current_commit == (server_mirror.current_commit - 1))
					{
						printf("old file. deleting from server...\n");
						// o arquivo é velho e deve ser deletado do servidor adequadamente.
						bzero(buffer, BUFFER_SIZE);
						buffer[0] = DELETE;
						write(socketfd, buffer, BUFFER_SIZE);

						bzero(buffer,BUFFER_SIZE);
						memcpy(buffer, server_mirror.fileinfo[i].name, MAXNAME);
						write(socketfd, buffer, BUFFER_SIZE);
					}
					else
					{
						printf("new file. downloading from server...\n");
						//isso quer dizer que é um arquivo novo colocado no servidor em outro pc.
						// pede para o servidor mandar o arquivo
						bzero(buffer, BUFFER_SIZE);
						buffer[0] = DOWNLOAD;
						write(socketfd, buffer, BUFFER_SIZE);

						bzero(buffer,BUFFER_SIZE);
						memcpy(buffer, server_mirror.fileinfo[i].name, MAXNAME);
						write(socketfd, buffer, BUFFER_SIZE);

						struct file_info f;
						//fica esperando receber struct
						bzero(buffer,BUFFER_SIZE);
						read(socketfd, buffer, BUFFER_SIZE);
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
						receive_file(fullpath, socketfd);

						//bota f na estrutura self
						insert_file_into_client_list(&self, f);
					}
				}
		    }
		}
		
		// avisa que acabou o seu sync.
		bzero(buffer, BUFFER_SIZE);
		buffer[0] = SYNC_END;
		write(socketfd, buffer, BUFFER_SIZE);

		//avança o estado de commit do cliente para o mesmo do servidor, já que ele atualizou.
		if(self.current_commit < (server_mirror.current_commit - 1)) {
			printf("oi\n");
			self.current_commit = server_mirror.current_commit; }
		else
			self.current_commit += 1;

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
	//strcpy(home,"/home/grad/");	//ufrgs
	strcat(home, getlogin());

	init_client(&self, home, argv[1]);
	
	// conecta este cliente com o servidor, que criará uma thread para administrá-lo
	socketfd = connect_server(argv[2], atoi(argv[3]));

	//manda userid para o server
	bzero(buffer, BUFFER_SIZE);
	memcpy(buffer, self.userid, MAXNAME);
	write(socketfd, buffer, BUFFER_SIZE);

	// recebe um ok do servidor para continuar a conexão
	bzero(buffer, BUFFER_SIZE);
	read(socketfd, buffer, BUFFER_SIZE);
	printf("%s\n",buffer);

	if(buffer[0] == 'A')
		printf("Connected. :)\n");
	else
	{
		printf("Connected in more than 2 devices.");
		close(socketfd);
		exit(1);
	}

	// conecta um novo socket na porta +1 para fazer o sync apenas sem bloquear o programa de comandos.
	sync_socketfd = connect_server(argv[2], atoi(argv[3])+1);

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
	pthread_create(&initial_sync_client, NULL, sync_client, (void*)newsync);
	pthread_detach(initial_sync_client);
    
	while(1) 
	{
		bzero(buffer, BUFFER_SIZE);
		fgets(buffer, BUFFER_SIZE, stdin);

		strcpy(message, buffer);

		char *command = strtok(message, " ");
		char *filepath = strtok(NULL, " ");

		if(strcmp(command, "list") == 0)
		{
			bzero(buffer, BUFFER_SIZE);
			buffer[0] = LIST;
			write(socketfd, buffer, BUFFER_SIZE);

			bzero(buffer, BUFFER_SIZE);
			read(socketfd, buffer, BUFFER_SIZE);

			// faz o que agora?
			
		}
		else if(strcmp(command, "exit") == 0)
		{
			bzero(buffer, BUFFER_SIZE);
			buffer[0] = EXIT;
			write(socketfd, buffer, BUFFER_SIZE);
		}
		else if(strcmp(command, "upload") == 0)
		{
			bzero(buffer, BUFFER_SIZE);
			buffer[0] = UPLOAD;
			write(socketfd, buffer, BUFFER_SIZE);

			// envia o arquivo;
		}
		else if(strcmp(command, "download") == 0)
		{
			bzero(buffer, BUFFER_SIZE);
			buffer[0] = DOWNLOAD;
			write(socketfd, buffer, BUFFER_SIZE);

			// enviar o nome do arquivo
			bzero(buffer, BUFFER_SIZE);
			memcpy(buffer, filepath, MAXNAME);
			write(socketfd, buffer, BUFFER_SIZE);

			// espera o arquivo
		}
	}

    return 0;
}
