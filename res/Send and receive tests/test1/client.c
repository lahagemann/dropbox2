#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <fcntl.h>

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
}

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












int main(int argc, char *argv[])
{
	int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
	
    char buffer[256];
    if (argc < 2) {
		fprintf(stderr,"usage %s hostname\n", argv[0]);
		return -1;
    }
	
	server = gethostbyname(argv[1]);
	if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        return -1;
    }
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        printf("ERROR opening socket\n");
    
	serv_addr.sin_family = AF_INET;     
	serv_addr.sin_port = htons(PORT);    
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);     
	
    
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        printf("ERROR connecting\n");

	send_file("teste.txt", sockfd);
	receive_file("teste2.txt", sockfd);
    
	close(sockfd);
    return 0;
}
