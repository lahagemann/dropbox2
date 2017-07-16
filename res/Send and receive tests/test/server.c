#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <fcntl.h>

#define PORT 12000




#include <string.h>

int file_size(FILE *file){
	int size = -1;
	long int current_position;

	if(file) {
		current_position = ftell(file);
		fseek(file, 0L, SEEK_END);
		size = ftell(file);
		fseek(file, current_position, SEEK_SET);
    }

    printf("SENT FILE SIZE = %d\n", size);
	return size;
}

int write_file(char* file_name, int socket){
	int i;
	long int size = -1;
	unsigned char *sizeBuffer;
    FILE *file;
	char buffer;

	// Faz a leitura do tamanho do arquivo, convertendo para int
	sizeBuffer = (unsigned char*)&size;
	if(read(socket, sizeBuffer, 4) > 0) {
        printf("\nLi do socket\n");
        if((file = fopen(file_name, "w+")) == NULL) {
            printf("ERROR on openning file: file doesn't exist.\n");
        } else {
            for(i=0; i<size; i++){
                read(socket, (void*)&buffer, 1);
                fputc(buffer, file);
            }
        }
		fclose(file);
	}
	return size;
}

void receive_file(char* file, int client_socket){
    char buffer[256];

	// Chama funcao que escreve arquivo
    int file_size = write_file(file, client_socket);
    printf("RECEIVED FILE SIZE = %d\n", file_size);
    
    if(file_size < 0){
        strcpy(buffer, "ERROR: file writing failed.");
        write(client_socket, buffer, sizeof(buffer));
        printf("ERROR on openning file: file doesn't exist.\n");
    }
    else {
		strcpy(buffer, "SUCCESS: file received.\n");
        write(client_socket, buffer, sizeof(buffer));
        printf("File %s downloaded.\n", file);
    }
}

void send_file(char *file, int client_socket) {
	FILE *fp;
	int offset;
	long int size;
	char buffer, server_answer[256];
	unsigned char* sizeBuffer;

	if(!(fp = fopen(file, "r"))) {
		printf("ERROR on openning file: file doesn't exist.\n");
	} else {
		size = file_size(fp);
		sizeBuffer = (unsigned char*) &size;

		// Se enviar o tamanho do arquivo
		if(write(client_socket, (void*)sizeBuffer, 4) > 0) {
			// Prepara envio do arquivo
			offset = 0;
			while(offset < size) {
				buffer = fgetc(fp);
				// Envia 1 byte do arquivo
				if(write(client_socket, (void*)&buffer, 1) < 0)
					break;
				offset++;
			}
		}

		printf("File %s uploaded.\n", file);
		fclose(fp);

		read(client_socket, server_answer, 256);
		printf("CLIENT RESPONSE: %s\n", server_answer);
	}
}






int main(int argc, char *argv[])
{
	int sockfd, newsockfd, n;
	socklen_t clilen;
	char buffer[256];
	struct sockaddr_in serv_addr, cli_addr;
	
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        printf("ERROR opening socket");
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);     
    
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		printf("ERROR on binding");
	
	listen(sockfd, 5);
	
	clilen = sizeof(struct sockaddr_in);
	if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) == -1) 
		printf("ERROR on accept");
	
	receive_file("teste.txt", sockfd);
	send_file("teste2.txt", sockfd);

	close(newsockfd);
	close(sockfd);
	return 0; 
}
