#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <fcntl.h>

#define PORT 12003




#include <string.h>
/*
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
}*/
/*
int write_file(char* file_name, int socket){
	int i;
	long int size = -1;
	unsigned char *sizeBuffer;
    FILE *file;
	char buffer[256];
printf("HERE\n");
	// Faz a leitura do tamanho do arquivo, convertendo para int
        bzero(buffer, 256);
	sizeBuffer = (unsigned char*)&size;
printf("HERE\n");
	if(read(socket, sizeBuffer, 4) > 0) {
                printf("\nLi do socket\n");
        if((file = fopen(file_name, "w+")) == NULL) {
            printf("ERROR on openning file: file doesn't exist.\n");
        } else {
                printf("file opened\n");
            for(i=0; i<size; i++){
                read(socket, (void*)&buffer, 1);
                fputc(buffer, file);
            }
        }
		fclose(file);
	}
	return size;
}*/
/*
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
}*/
/*
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

*/


void receive_file(int client_socket){
    char buffer[256], file_name[256], char_buffer[1];
    int error;
    FILE *fp;

    //receiving file name    
    bzero(buffer, 256);
    bzero(file_name, 256);
    error = read(client_socket, buffer, 256);
    if (error < 0){printf("ERROR GETTING FILE NAME"); return;}
    strncpy(file_name, buffer, strlen(buffer));

    //open file
    fp = fopen(file_name, "w");


    bzero(char_buffer, 1);
    read(client_socket, char_buffer, 1);
    //fputc(char_buffer[0], fp);
    while(char_buffer[0] != EOF)
    {
        fputc(char_buffer[0], fp);
        bzero(char_buffer, 1);
        read(client_socket, char_buffer, 1);
        
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
	
    int port = atoi(argv[1]);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);     
    
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		printf("ERROR on binding");
	
	listen(sockfd, 5);
	
	clilen = sizeof(struct sockaddr_in);
	if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) == -1) 
		printf("ERROR on accept");

    receive_file(newsockfd);

	
/***
	bzero(buffer, 256);
	
	// read from the socket
	n = read(newsockfd, buffer, 256);
	if (n < 0) 
		printf("ERROR reading from socket");
	printf("Here is the message: %s\n", buffer);
	
	// write in the socket
	n = write(newsockfd,"I got your message", 18);
	if (n < 0) 
		printf("ERROR writing to socket");
*****/

	close(newsockfd);
	close(sockfd);
	return 0;  
}
