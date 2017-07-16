void list_files(fileinfo[]){

}



void remove_file(){

}



int return_client(char* user_name, client *new_client){
    client *tempinfo;

	if(FirstFila2(&connected_clients) != 0){
		do{
			tempinfo = (client*)(GetAtIteratorFila2(&connected_clients));
			if (strcmp(tempinfo->userid, user_name) == 0){ //mesmo id = já tem um logado
			        new_client = (client*)(GetAtIteratorFila2(&connected_clients));
					return ACCEPTED;	
            }			
            else{
					return NOT_VALID;
	        }
			
				
		}while(NextFila2(&connected_clients) != 0);
    }
}
