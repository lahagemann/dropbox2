#include "../include/dropboxUtil.h"
//#include "dropboxUtil.h"

void init_client(client *client, char *home, char *login)
{
	//inicializa estrutura self do cliente
	strcpy(client->userid, login);
	
	//printf("client login %s\n", client->userid);
	//inicializa todos os nomes de arquivo como empty string, preparando para a função update client
	int i;
	for(i = 0; i < MAXFILES; i++)
		strcpy(client->fileinfo[i].name, "\0");
	
	//verifica se o diretorio sync_dir existe na home do usuario. se nao existir, cria.
	char sync_dir[256];
	strcpy(sync_dir, home);
	strcat(sync_dir, "/sync_dir_");
	strcat(sync_dir, login);

	//printf("syncdir: %s\n", sync_dir);

	struct stat st;
	if (stat(sync_dir, &st) != 0) {
		  mkdir(sync_dir, 0777);
	}

	client->logged_in = 1;
	client->current_commit = 0;
	client->devices[0] = 0;

    if (pthread_mutex_init(&(client->mutex), NULL) != 0)
    {
        printf("\nMutex (queue) init failed\n");
        return;
    }
	if (pthread_cond_init(&(client->cond), NULL) != 0)
    {
        printf("\nMutex (queue) init failed\n");
        return;
    }

	update_client(client, home);
}

void update_client(client *client, char *home)
{
	//setup do nome do diretório em que ele precisa procurar.
	int i = 0;

	char sync_dir[256];
	strcpy(sync_dir,home);
	strcat(sync_dir,"/sync_dir_");
	strcat(sync_dir,client->userid);

	struct dirent *dir;	
	DIR *d;
	
	// lista todos os arquivos no diretorio sync_dir do usuario, colocando-os na estrutura self do cliente
	d = opendir(sync_dir);
	if (d)
	{
	  while ((dir = readdir(d)) != NULL)
	  {
			struct file_info fi;
			
			if(strcmp(dir->d_name, "..") == 0 || strcmp(dir->d_name, ".") == 0);
			else
			{
				// d_name é o nome do arquivo sem o resto do path. ta de boasssss
				char copy[256];
				strcpy(copy, dir->d_name);
				char *name = strtok(copy, ".");
				char *extension = strtok(NULL, ".");
		
			  	strcpy(fi.name, name);
			  	strcpy(fi.extension, extension);


				char *til = strchr(fi.extension, '~');
				int empty_name = strcmp(fi.name, "");
				if(til || empty_name == 0);
				else
				{

				  	// pegar ultima data de modificação do arquivo
					//STAT É CHAMADO COM FULL PATH, TEM QUE CONCATENAR
					char fullpath[256];
					strcpy(fullpath, sync_dir);
					strcat(fullpath, "/");
					strcat(fullpath, name);
					strcat(fullpath, ".");
					strcat(fullpath, extension);

				  	struct stat attrib;
				  	stat(fullpath, &attrib);
		            strftime(fi.last_modified, MAXNAME, "%d-%m-%Y-%H-%M-%S", localtime(&(attrib.st_ctime)));
				  	//strftime(fi.last_modified, MAXNAME, "%d-%m-%Y-%H-%M-%S", localtime(&(attrib.st_ctime)));
				  	// strftime(date, 20, "%d-%m-%y", localtime(&(attrib.st_ctime)));
				  	fi.size = (int)attrib.st_size;

					//leitura do arquivo está sendo feita neste commit. colocar o commit atual do cliente no arquivo

					fi.commit_modified = client->current_commit;

					int index = search_files(client, name);

					if(index >= 0) // arquivo já existe na estrutura
					{
						file_info f;
						memcpy(&f, &(client->fileinfo[index]), sizeof(file_info));
				
						// se a data de modificação do arquivo que eu to lendo agora for mais recente que o que ja tava na estrutura self, sobrescrever.
						if(file_more_recent_than(fi, f)) 
						{
							memcpy(&client->fileinfo[index], &fi, sizeof(file_info));
						}
					}
					else
					{
						//arquivo não está na estrutura, adicionar.
						insert_file_into_client_list(client, fi);
					}
				}
			}
		  	i++;
	  }
	  closedir(d);
	}

	// loop de deleção de elementos da estrutura fileinfo que não existem mais no diretório
	for(i = 0; i < MAXFILES; i++)
	{
		if(strcmp(client->fileinfo[i].name, "\0") == 0)
			break;
		else
		{
			char file[256];
			strcpy(file, sync_dir);
			strcat(file, "/");
			strcat(file, client->fileinfo[i].name);
			strcat(file, ".");
			strcat(file, client->fileinfo[i].extension);

			//stat: On success, zero is returned.  On error, -1 is returned, and errno is set appropriately.
			struct stat st;
			if (stat(file, &st) != 0) {
			  delete_file_from_client_list(client, client->fileinfo[i].name);
			}
		}
	}

}

int search_files(client *client, char filename[MAXNAME])
{
	int i;
	for(i = 0; i < MAXFILES; i++)
	{
		if(strcmp(client->fileinfo[i].name,"\0") != 0)
		{
			if(strcmp(filename, client->fileinfo[i].name) == 0)
				return i;
		}
	}
	return -1;
}

void insert_file_into_client_list(client *client, file_info fileinfo)
{
	int i = 0;

	while(strcmp(client->fileinfo[i].name,"\0") != 0)
		i++;

	memcpy(&client->fileinfo[i], &fileinfo, sizeof(file_info));
	//printf("file: %s\n",client->fileinfo[i].name);
	
	
	/*for(i = 0; i < MAXFILES; i++)
	{
		// bota na primeira posição livre que achar.
		if(strcmp(client->fileinfo[i].name,"\0") == 0)
		{
			printf("inserted file into client list\n");
			
			memcpy(&client->fileinfo[i], &fileinfo, sizeof(file_info));
			//printf("file: %s\n",client->fileinfo[i].name);
			printf("fileinfo: %s %s %d\n", client->fileinfo[i].name, client->fileinfo[i].extension, client->fileinfo[i].commit_modified);
			break;
		}
	}*/
}

void delete_file_from_client_list(client *client, char filename[MAXNAME])
{
	int index = search_files(client, filename);
	if(index >= 0)
	{	
		strcpy(client->fileinfo[index].name,"\0");
		strcpy(client->fileinfo[index].extension,"\0");
		strcpy(client->fileinfo[index].last_modified,"\0");
		client->fileinfo[index].commit_modified = 0;
		client->fileinfo[index].size = 0;
	}
}

// retorna 1 se f1 é mais recente que f2. retorna 0  caso contrário
int file_more_recent_than(file_info f1, file_info f2)
{
	/* DATE FORMAT
		"%d-%m-%Y-%H-%M-%S"
		31-01-2001-13-14-23
	*/

	//copia strings pra não serem destruídas pelo strtok caso seja por ref.
	char f1_lm[256], f2_lm[256];
	strcpy(f1_lm, f1.last_modified);
	strcpy(f2_lm, f2.last_modified);

	int f1_day = atoi(strtok(f1_lm, "-"));
	int f1_month = atoi(strtok(NULL, "-"));
	int f1_year = atoi(strtok(NULL, "-"));
	int f1_hour = atoi(strtok(NULL, "-"));
	int f1_minutes = atoi(strtok(NULL, "-"));
	int f1_seconds = atoi(strtok(NULL, "-"));

	int f2_day = atoi(strtok(f2_lm, "-"));
	int f2_month = atoi(strtok(NULL, "-"));
	int f2_year = atoi(strtok(NULL, "-"));
	int f2_hour = atoi(strtok(NULL, "-"));
	int f2_minutes = atoi(strtok(NULL, "-"));
	int f2_seconds = atoi(strtok(NULL, "-"));

	if(f1_year > f2_year)
		return 1;
	else if (f2_year > f1_year)
		return 0;
	else
	{
		if(f1_month > f2_month)
			return 1;
		else if (f2_month > f1_month)
			return 0;
		else
		{
			if(f1_day > f2_day)
				return 1;
			else if(f2_day > f1_day)
				return 0;
			else 
			{
				if(f1_hour > f2_hour)
					return 1;
				else if(f2_hour > f1_hour)
					return 0;
				else
				{
					if(f1_minutes > f2_minutes)
						return 1;
					else if(f2_minutes > f1_minutes)
						return 0;
					else 
					{
						if(f1_seconds > f2_seconds)
							return 1;
						else
							return 0;
					}
				}
			}
		}
	}
}

void receive_file(char* file_name, int client_socket)
{
    char buffer[256], char_buffer[1];
    int error;

	struct stat st;
	if (stat(file_name, &st) == 0) {
		  unlink(file_name);
	}

    FILE *fp;

    //printf("%s", file_name);
    //open file
    fp = fopen(file_name, "w");


    bzero(char_buffer, 1);
    read(client_socket, char_buffer, 1);
	//printf("%c\n", char_buffer[0]);
    //fputc(char_buffer[0], fp);
    while(char_buffer[0] != 25)
    {
        fprintf (fp, "%c", char_buffer[0]);
        bzero(char_buffer, 1);
        read(client_socket, char_buffer, 1);
		//printf("%d\n", char_buffer[0]);
        
	}

    fclose(fp);
    
}

void send_file(char *file, int server_socket)
{
	char string[2], buffer[1];
	FILE *fp;

	//abrir arquivo para leitura
	fp = fopen(file, "rb");

	if(fp == NULL)
		perror("error\n");

	while(fgets(string, 2, fp))
	{
		bzero(buffer, 1);
		buffer[0] = string[0];
		//printf("%c", buffer[0]);
		write(server_socket, buffer, 1);
	}
	
	bzero(buffer, 1);
	buffer[0] = 25;
	write(server_socket, buffer, 1);

	fclose(fp);
}

void remove_file(char *filename)
{
	unlink(filename);
}


