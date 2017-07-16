//DECLARAR GLOBAL
	pthread_mutex_t queue;
	PFILA2 connected_clients;
	
//INICIALIZAR NA MAIN DO SERVIDOR (antes de entrar no loop de esperar clientes)
	if (pthread_mutex_init(&queue, NULL) != 0)
    {
        printf("\nMutex (queue) init failed\n");
        return 1;
    }
	
	if(CreateFila2(connected_clients) != 0){
		printf("\nInit failed\n");
		return 1;
	}

//FUNCAO
void insert_client(client clientinfo){
	pthread_mutex_lock(&queue);
	
	//busca se já existe
	client *tempinfo;
	if(FirstFila2(connected_clients) != 0){
		do{
			tempinfo = (client*)(GetAtIteratorFila2(connected_clients));
			if (strcmp(tempinfo->userid, clientinfo.userid) == 0){ //mesmo id = já tem um logado
				if(tempinfo->devices[1] == 1) //terceiro cliente não pode logar
					return NOT_VALID;	
				else{
					tempinfo->devices[1] = 1;
					return ACCEPTED;
				}
			}
				
		}while(NextFila2(connected_clients) != 0);
	}
	else{ //fila vazia pode inserir
		AppendFila2(connected_clients, (void*)clientinfo);
		tempinfo->devices[0] = 1;
		return ACCEPTED;
	}
	
	//nao achou cliente na fila, insere no fim
	AppendFila2(connected_clients, (void*)clientinfo);
	tempinfo->devices[0] = 1;
	
	pthread_mutex_unlock(&queue);
}