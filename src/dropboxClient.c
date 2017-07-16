#include "../include/dropboxClient.h"
#include "../include/dropboxUtil.h"
//#include "dropboxClient.h"
//#include "dropboxUtil.h"

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
		// faz isso a cada x segundos:
		sleep(SLEEP);

		printf("syncing...\n");
		
		// AQUI ETAPA DO SYNC_SERVER!
		update_client(&self, home);

		// envia seu mirror pro servidor
		bzero(buffer, BUFFER_SIZE);
		memcpy(buffer, &self, sizeof(struct client));
		write(socketfd, buffer, sizeof(struct client));

		while(1)
		{
			char command;
			char fname[MAXNAME];

			bzero(buffer,BUFFER_SIZE);
			read(socketfd, buffer, 1);
			memcpy(&command, buffer, 1);

			if(command == DOWNLOAD)
			{
				// recebe nome do arquivo
				bzero(buffer,BUFFER_SIZE);
				read(socketfd, buffer, MAXNAME);
				memcpy(fname, buffer, MAXNAME);
				
				// procura arquivo
				int index = search_files(&self, fname);
				file_info f;
				if(index >= 0)
					memcpy(&f, &self.fileinfo[index], sizeof(file_info));

				// manda struct
				bzero(buffer,BUFFER_SIZE);
				memcpy(buffer, &f, sizeof(file_info));
				write(socketfd, buffer, sizeof(file_info));
				
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
				read(socketfd, buffer, MAXNAME);
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
	
		update_client(&self, home);
	
		// envia para o servidor que ele vai começar o sync.
		bzero(buffer, BUFFER_SIZE);
		buffer[0] = SYNC;
		write(socketfd, buffer, 1);
		
		// envia para o servidor o seu login
		bzero(buffer,BUFFER_SIZE);
		memcpy(buffer, self.userid, MAXNAME);
		write(socketfd, buffer, MAXNAME);

	  	// fica esperando o servidor enviar sua estrutura deste client.
		int n = 0;
		bzero(buffer,BUFFER_SIZE);
		while(n < sizeof(struct client))
			n += read(socketfd, buffer+n, 1);
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

				if(index >= 0)		// arquivo existe no cliente
				{
					//verifica se o arquivo no servidor é mais atual que o arquivo no cliente.
					if(server_mirror.fileinfo[i].commit_modified > self.fileinfo[index].commit_modified)
					{
						//isso quer dizer que o arquivo no servidor é de um commit mais novo que o estado atual do cliente.
						// pede para o servidor mandar o arquivo
						bzero(buffer, BUFFER_SIZE);
						buffer[0] = DOWNLOAD;
						write(socketfd, buffer, 1);

						bzero(buffer,BUFFER_SIZE);
						memcpy(buffer, server_mirror.fileinfo[i].name, MAXNAME);
						write(socketfd, buffer, MAXNAME);
					
						struct file_info f;
						//fica esperando receber struct
						bzero(buffer,BUFFER_SIZE);
						int n = 0;
						while(n < sizeof(struct file_info)){
							n += read(socketfd, buffer+n, 1	);}
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
					if(self.current_commit == (server_mirror.current_commit - 1))
					{
						// o arquivo é velho e deve ser deletado do servidor adequadamente.
						bzero(buffer, BUFFER_SIZE);
						buffer[0] = DELETE;
						write(socketfd, buffer, 1);

						bzero(buffer,BUFFER_SIZE);
						memcpy(buffer, server_mirror.fileinfo[i].name, MAXNAME);
						write(socketfd, buffer, MAXNAME);
					}
					else
					{
						//isso quer dizer que é um arquivo novo colocado no servidor em outro pc.
						// pede para o servidor mandar o arquivo
						bzero(buffer, BUFFER_SIZE);
						buffer[0] = DOWNLOAD;
						write(socketfd, buffer, 1);

						bzero(buffer,BUFFER_SIZE);
						memcpy(buffer, server_mirror.fileinfo[i].name, MAXNAME);
						write(socketfd, buffer, MAXNAME);

						struct file_info f;
						//fica esperando receber struct
						bzero(buffer,BUFFER_SIZE);
						int n = 0;
						bzero(buffer,BUFFER_SIZE);
						while(n < sizeof(struct file_info))
							n += read(socketfd, buffer+n, 1);
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
		write(socketfd, buffer, 1);

		//avança o estado de commit do cliente para o mesmo do servidor, já que ele atualizou.
		self.current_commit = server_mirror.current_commit; 

		printf("ending sync.\n");
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

	//strcpy(home,"/home/"); //home
	strcpy(home,"/home/grad/");	//ufrgs
	strcat(home, getlogin());
	
	init_client(&self, home, argv[1]);
	
	// conecta este cliente com o servidor, que criará uma thread para administrá-lo
	socketfd = connect_server(argv[2], atoi(argv[3]));

	//manda userid para o server
	bzero(buffer, BUFFER_SIZE);
	memcpy(buffer, self.userid, MAXNAME);
	write(socketfd, buffer, MAXNAME);

	// recebe um ok do servidor para continuar a conexão
	bzero(buffer, BUFFER_SIZE);
	read(socketfd, buffer, 1);


	if(buffer[0] == 'A')
		printf("Connected. :)\n");
	else
	{
		printf("Connected in more than 2 devices. Disconnecting...");
		close(socketfd);
		exit(1);
	}

	// recebe quantos clientes estão conectados para saber em que +x porta deve conectar
	bzero(buffer, BUFFER_SIZE);
	read(socketfd, buffer, 1);
	int clients = buffer[0];

	// conecta um novo socket na porta +1 para fazer o sync apenas sem bloquear o programa de comandos.
	sync_socketfd = connect_server(argv[2], atoi(argv[3])+clients);

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

	bzero(buffer, BUFFER_SIZE);
	memcpy(buffer, self.userid, MAXNAME);
	write(socketfd, buffer, MAXNAME);
    
	while(1) 
	{
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
			write(socketfd, buffer, 1);

			// AQUI DÁ PROBLEMA, TEM QUE LER TODO O BUFFER
			int n = 0;
			bzero(buffer, BUFFER_SIZE);
			while(n < sizeof(struct client))
				n += read(socketfd, buffer+n, 1);

			printf("%s", buffer);
		}
		else if(strcmp(command, "exit") == 0)
		{
			bzero(buffer, BUFFER_SIZE);
			buffer[0] = EXIT;
			write(socketfd, buffer, 1);
			
			close(socketfd);
			exit(0);
		}
		else if(strcmp(command, "upload") == 0)
		{
			bzero(buffer, BUFFER_SIZE);
			buffer[0] = UPLOAD;
			write(socketfd, buffer, 1);

			bzero(buffer, BUFFER_SIZE);
			memcpy(buffer, filepath, 256);
			write(socketfd, buffer, 256);
		
			// envia o arquivo;
			send_file(filepath, socketfd);

			printf("File uploaded.\n");
		}
		else if(strcmp(command, "download") == 0)
		{
			bzero(buffer, BUFFER_SIZE);
			buffer[0] = DOWNLOAD;
			write(socketfd, buffer, 1);

			// enviar o nome do arquivo
			// separar com strtok a extensão do arquivo

			char copy[256];
			strcpy(copy, filepath);

			char *name = strtok(copy, ".");

			bzero(buffer, BUFFER_SIZE);
			memcpy(buffer, name, MAXNAME);
			write(socketfd, buffer, MAXNAME);

			// espera mensagem de ok do servidor:
			bzero(buffer, BUFFER_SIZE);
			read(socketfd, buffer, 1);

			if(buffer[0] == FILE_FOUND)
			{
				// pega o diretório Downloads do usuário e baixa para lá.
				char user_downloads_dir[256];
				//strcpy(user_downloads_dir, "/home/");
				strcpy(user_downloads_dir, "/home/grad/");
				strcat(user_downloads_dir, getlogin());
				strcat(user_downloads_dir, "/Downloads/");
				strcat(user_downloads_dir, filepath);			

				// espera o arquivo
				receive_file(user_downloads_dir, socketfd);

				printf("Downloaded file to /Downloads.\n");
			}
			else
			{
				printf("The file you asked for doesn't exit. Please try again.\n");
			}
		}
	}

    return 0;
}
