#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#define PORT 4000
typedef struct operation {
	char operador;
	float operando1;
	float operando2;
} operation;

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, n;
	socklen_t clilen;
	char buffer[256];
	struct sockaddr_in serv_addr, cli_addr;
	operation op;

	if(argc < 2) {
		printf("enter port\n");
		exit(1);
	}
	
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        	printf("ERROR opening socket");
		close(sockfd);
		exit(1);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		printf("ERROR on binding");
		close(sockfd);
		exit(1);
	}

	clilen = sizeof(struct sockaddr_in);

	bzero(buffer, 256);

	while(1) {
		//int recvfrom(int sockfd, void *buf, int len, unsigned int flags,struct sockaddr *from, int *fromlen); 
		if(recvfrom(sockfd, buffer, 256, 0, (struct sockaddr *)&cli_addr, &clilen) == -1) {
			printf("error receiving");
			close(sockfd);
			exit(1);
		}

		//pegar os dados do buffer
		memcpy(&op, buffer, sizeof(operation));


		float result = 0.0f;
		//realizar a operação
		switch(op.operador) {
			case '+': 	result = op.operando1 + op.operando2; break;
			case '-': 	result = op.operando1 - op.operando2; break;
			case '*': 	result = op.operando1 * op.operando2; break;
			case '/': 	if(op.operando2 != 0) { result = op.operando1 / op.operando2; break; }
					else break;
			default: 	break;

		}
		sprintf(buffer, "%f", result);

		//enviar de volta o resultado com sendto
		//sendto(int socket, const void *buffer, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
		if(sendto(sockfd, buffer, 256, 0, (struct sockaddr *)&cli_addr, clilen) == -1) {
			printf("error sending");
			close(sockfd);
			exit(1);
		}
	}

	return 0;
}
