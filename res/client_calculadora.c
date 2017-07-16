#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

typedef struct operation {
	char operador;
	float operando1;
	float operando2;
} operation;

int main(int argc, char *argv[])
{
	int sockfd, n;
    	struct sockaddr_in serv_addr;
	socklen_t servlen;
    	struct hostent *server;
	operation op;

	//char IP[256];
	
    	char buffer[256];
    	if (argc < 3) {
		fprintf(stderr,"usage %s hostname\n", argv[0]);
		exit(0);
    	}
	
	// get server address
	server = gethostbyname(argv[1]);
	if (server == NULL) {
        	fprintf(stderr,"ERROR, no such host\n");
        	exit(0);
    	}

	// open connection
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        	printf("ERROR opening socket");
		close(sockfd);
		exit(1);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[2]));
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);

	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        printf("ERROR connecting\n");

	servlen = sizeof(struct sockaddr_in);
	while(1) {
		bzero(buffer, 256);
	    printf("Enter the operation: ");
		scanf(" %c", &op.operador);
		printf("\nEnter op1: ");
		scanf("%f", &op.operando1);
		printf("\nEnter op2: ");
		scanf("%f", &op.operando2);

		memcpy(buffer, &op, sizeof(operation));
	    
		/* write in the socket */
		if(sendto(sockfd, buffer, 256, 0, (struct sockaddr *)&serv_addr, servlen) == -1) {
			printf("error sending");
			close(sockfd);
			exit(1);
		}

	    bzero(buffer,256);

		// read answer
		//int recvfrom(int sockfd, void *buf, int len, unsigned int flags,struct sockaddr *from, int *fromlen); 
		if(recvfrom(sockfd, buffer, 256, 0, (struct sockaddr *)&serv_addr, &servlen) == -1) {
			printf("error receiving");
			close(sockfd);
			exit(1);
		}

    	printf("%s\n",buffer);
	}

	return 0;
}
