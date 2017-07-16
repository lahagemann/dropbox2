void run_thread(void *socket_client)
{
	char buffer[BUFFER_SIZE];
	int socketfd = *(int*)socket_client;
	int message;
	printf("i created a thread\n");
	

	while(1) {
		bzero(buffer, BUFFER_SIZE);
		message = read(socketfd, buffer, BUFFER_SIZE);
		if (message < 0) 
			printf("ERROR reading from socket");
		else {					
			//nao sei se é exatamente assim
			switch(message){
				case EXIT:
					disconnect_client(client);
					pthread_exit();
					break;
				case SYNC:
					sync_server();
					break;
				case DOWNLOAD:
					//tem que ver como vamos receber isso...
					message = read(socketfd, buffer, BUFFER_SIZE);
					send_file(message, socketfd);
					break;
				case UPLOAD:
					message = read(socketfd, buffer, BUFFER_SIZE);
					receive_file(message, socketfd);
					break;
			}
		}
		
	}

	free(socket_client);

	close(socketfd);
}