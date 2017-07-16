void disconnect_client(client clientinfo){
	pthread_mutex_lock(&queue);
	
	client *tempinfo;
	tempinfo = (client*)(GetAtIteratorFila2(connected_clients));
	while(strcmp(tempinfo->userid, clientinfo.userid) != 0){
		NextFila2(connected_clients);
		tempinfo = (client*)(GetAtIteratorFila2(connected_clients));
	}
		
	//se tem dois conectados, disconecta um
	if(tempinfo->devices[1] == 1)
		tempinfo->devices[1] = 0;
	else //se não, remove a estrutura
		DeleteAtIteratorFila2(connected_clients);
		
	pthread_mutex_unlock(&queue);
}