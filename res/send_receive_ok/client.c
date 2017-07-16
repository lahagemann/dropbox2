#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <fcntl.h>

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
}*/
/*
void receive_file(char* file, int server_socket){
    char buffer[256];
    
    // Chama funcao que escreve arquivo
    int file_size = write_file(file, server_socket);
    printf("RECEIVED FILE SIZE = %d\n", file_size);
    
    if(file_size < 0){
        strcpy(buffer, "ERROR: file writing failed.");
        write(server_socket, buffer, sizeof(buffer));
        printf("ERROR on openning file: file doesn't exist.\n");
    }
    else {
        strcpy(buffer, "SUCCESS: file received.\n");
        write(server_socket, buffer, sizeof(buffer));
        printf("File %s downloaded.\n", file);
    }
}*/
/*
void send_file(char *file, int server_socket) {
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
		if(write(server_socket, (void*)sizeBuffer, 4) > 0) {
			// Prepara envio do arquivo
			offset = 0;
			while(offset < size) {
				buffer = fgetc(fp);
				// Envia 1 byte do arquivo
				if(write(server_socket, (void*)&buffer, 1) < 0)
					break;
				offset++;
			}
		}

		printf("File %s uploaded.\n", file);
		fclose(fp);

		read(server_socket, server_answer, 256);
		printf("SERVER RESPONSE: %s\n", server_answer);
	}
}


*/



void send_file(char *file, int server_socket) {
    char buffer[256], char_buffer, cb[1];
    int error;
    FILE *fp;

    //sending file name
    bzero(buffer, 256);
    sprintf(buffer, "%s", file);
    error = write(server_socket, buffer, strlen(buffer));
    if (error < 0) {printf("ERROR writing to socket\n"); return;}

    //abrir arquivo para leitura
    fp = fopen(file, "r");

    //passar char a char
    char_buffer =  fgetc(fp);
    while(char_buffer != EOF)
    {    
        bzero(cb, 1);
        cb[0] = char_buffer;
        write(server_socket, cb, 1);
        char_buffer =  fgetc(fp);
    }

    bzero(cb, 1);
    cb[0] = char_buffer;
    write(server_socket, cb, 1);
}





int main(int argc, char *argv[])
{
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
	
    char buffer[256];
    if (argc < 2) {
		fprintf(stderr,"usage %s hostname\n", argv[0]);
		exit(0);
    }
	
	server = gethostbyname(argv[1]);
	if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    int port = atoi(argv[2]);
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        printf("ERROR opening socket\n");
    
	serv_addr.sin_family = AF_INET;     
	serv_addr.sin_port = htons(port);    
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);     
	
    
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        printf("ERROR connecting\n");

    send_file("teste1.txt", sockfd);




/**
    printf("Enter the message: ");
    bzero(buffer, 256);
    fgets(buffer, 256, stdin);
    
	// write in the socket
	n = write(sockfd, buffer, strlen(buffer));
    if (n < 0) 
		printf("ERROR writing to socket\n");

    bzero(buffer,256);

	// read from the socket
    n = read(sockfd, buffer, 256);
    if (n < 0) 
		printf("ERROR reading from socket\n");

    printf("%s\n",buffer);
    
	
****/

    close(sockfd);
    return 0;
}
