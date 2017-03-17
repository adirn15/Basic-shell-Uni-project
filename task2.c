#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include "LineParser.h"
#include <sys/types.h>
#include <sys/wait.h>
#define MAX_PATH 2048
#define HISTORY_ARR 3
#define MAX_COM_LENGTH 100


typedef struct link1 link1;
 
struct link1 {
    char* var;
    char* value;
    link1 *next;
};


int debug=0;
char* cur_dir;
char* hist_com;
char* history[HISTORY_ARR*sizeof(char*)];
int history_index = 0; 
int history_size = 0;
link1* var_list=0;

link1* list_append_last(link1*,link1*);
void list_print(link1*);  
void list_free(link1*); 
void print_link_data(link1*);
void execute(cmdLine*,char*);
void cd(cmdLine*);
void print_history();
void free_history();
cmdLine* apply_history_command(cmdLine*);
int delete_old_var(char*);
void set_com(cmdLine*);
void delete_var(cmdLine*);
int replace_args(cmdLine*);

int main (int argc, char** argv){
	int i;
	for (i=0; i<HISTORY_ARR; i++){
		history[i]=0;
	}
	for (i=0; i<argc; i++){
		if (strcmp(argv[i],"-d")==0){
			debug=1;
			printf("Debug Mode\n");
		}
	}
	while(1){
    	cur_dir = (char*)malloc(MAX_PATH);
		getcwd(cur_dir,MAX_PATH);                   
		printf("working directory: %s\n", cur_dir);
		printf("\n***enter command***\n\n");
		char* user_buff;
		user_buff=(char*)malloc(MAX_PATH);
		fgets(user_buff,MAX_PATH,stdin);
		cmdLine* cmdline;
		cmdline=parseCmdLines(user_buff);
		char* command = cmdline->arguments[0];
		hist_com = (char*)malloc(MAX_COM_LENGTH);
		if ((strlen(command)>0)&&command[0]=='!'){
	    	cmdline = apply_history_command(cmdline);
		}
		if (cmdline==0){
			perror("breaking\n");
		}
		else if (strcmp("set",command)==0){
			set_com(cmdline);
		}
		else if (strcmp("quit",command)==0){ /*the command is quit*/
			free(user_buff);
			free(cur_dir);	
			free_history();
			freeCmdLines(cmdline);
			list_free(var_list);

			return 0;
		}
		else execute(cmdline,hist_com);
		free(user_buff);
		free(cur_dir);
		freeCmdLines(cmdline);
	}
} 

void execute (cmdLine *pCmdLine,char* cur_com){
	int* statusPtr=0;
	char* current_command = pCmdLine->arguments[0];

	
	/***********************************************************/
	/********************add to history*************************/

	int append_size = 0;
	int j;
	for (j=0; j<pCmdLine->argCount; j++){
		strcpy(cur_com+append_size,pCmdLine->arguments[j]);
		append_size+=strlen(pCmdLine->arguments[j]);
		strcpy(cur_com+append_size," ");
		append_size++;
	} /* append history command as string*/
	if (history_index<HISTORY_ARR){		
		history[history_index++]=cur_com;
		history_index=history_index%HISTORY_ARR;
		history_size++;
	}
	else{ /*history_size = HISTORY_ARR*/
		free(history[history_index]);
		history[history_index++]=cur_com;
		history_index%=HISTORY_ARR;
	}

	/*************replace variable arguments**********************/
	/*************************************************************/

	if (replace_args(pCmdLine)<0){ /*switch vars to saved value*/
		printf("Error switching values of vars");
		return;
	} 


	/***************functions for parent *************************/
	/*************************************************************/
	if (debug){
		fprintf(stderr,"Parent Process:\np_id = %d\n",getpid());
		fprintf(stderr,"command = %s\n\n",pCmdLine->arguments[0]);
	}

	if (strcmp(current_command,"delete")==0){
		delete_var(pCmdLine);
		return;
	}
	else if (strcmp(current_command,"cd")==0){
		cd(pCmdLine);
		return;
	}

	/*******************execute the command***********************/
	/*************************************************************/

	pid_t pid = fork();

	/* pid<0: error, pid=0: child, pid>0: parent */ 
	if (pid<0){
		perror("Error forking process");
		exit(2);
	}
	else if (pid==0){ /*child process*/
		if (debug){
			fprintf(stderr,"Child Process:\np_id = %d\n",getpid());
			fprintf(stderr,"command = %s\n\n",pCmdLine->arguments[0]);
		}
		if (strcmp(current_command,"history")==0){
			print_history();
			_exit(1);
		}
		else if (strcmp(current_command,"set")==0){
			set_com(pCmdLine);
			_exit(1);
		}
		else if (strcmp(current_command,"env")==0){
			printf("\nLIST OF VARIABLES: \n");
			list_print(var_list);
			printf("\n\n");
		    _exit(1);
		}
		else if (execvp(current_command,pCmdLine->arguments)){
			perror("Error executing command");
			_exit(1);
		}
	}
	else if (pid>0){ /*   pid>0 -> parent process   */
		if (pCmdLine->blocking==1)
			waitpid(-1,statusPtr,0); /* -1: wait for all child procesess. 0 = options- no particular (WNOHANF,WUNTRACED,WCONTINUED)
											statusptr = returns to the pointer the status info of running process */ 
		if (debug){
			printf("Child finished - Parent process returning to loop\n\n");
		}
		return;
	}

}

void cd(cmdLine* cmdline){
	char* const command = cmdline->arguments[0];
	if (strcmp("cd",command)==0){
		if (cmdline->argCount==1){ /*cd only = one dir back*/
			perror("Illegal cd command");
		}
		else if (cmdline->argCount==2 && strcmp(cmdline->arguments[1],"~")==0){
			if (chdir(getenv("HOME"))==-1){
			   	perror("Error executing cd command");
			}
		}
		else if (chdir(cmdline->arguments[1]) == -1){
            	perror("Error executing cd command");
		}
	}
}

void print_history(){
	int i;
	int index= history_index;
	for (i=0; i<HISTORY_ARR; i++){
		if (history[index]==0)
			break;
		printf("# %d: %s\n",i,history[index]);
		index=(index+1)%HISTORY_ARR;
	}
}

cmdLine* apply_history_command(cmdLine* cmdline){
	char* com =cmdline->arguments[0];
	com++;
	char str1[strlen(com)+1];
	strcpy(str1,com);
	int i;
	int index=(history_index-1)%HISTORY_ARR;
	printf("%s %d history index\n",history[index],index);
	for (i=0; i<history_size; i++){
		char str2[strlen(history[index])+1];
		strcpy(str2,history[index]);
		if (strncmp(str1, str2,strlen(str1))==0){
			char* newline= history[index];
			freeCmdLines(cmdline);
			cmdline=0;
			return parseCmdLines(newline);
		}
		index++;
		index=index%HISTORY_ARR;
	}
	perror("Error: Illegal input in command");
	return 0;
}

void free_history(){
	int i;
	for (i=0; i<history_size; i++){
		free(history[i]);
	}
}

void list_print(link1* list){
	link1* curr=list;
	if (curr==NULL)
		return;
	while (curr!=NULL){
		print_link_data(curr);
		curr=curr->next;
	}
}

void print_link_data(link1* link){
	printf("Variable: %s, Value: %s\n",link->var,link->value);
}

link1* list_append_last(link1* list, link1* data){
	link1* first = list;
	link1* prev;
	if (list==NULL){
		return data;
	}
	else if (list->next==NULL){
		list->next= data;
		return first;
	}
	else{
		prev=first;
		while (prev->next!=0){
			prev=prev->next;
		}
		prev->next=data;
		return first;
	}
}




void list_free(link1* list){
	if (list!=0){
		free(list->var);
		free(list->value);
		list_free(list->next);
		free(list);
	}
}

void delete_var(cmdLine* c){
	if (c->argCount!=2){
		perror("Error: bad structure of command\n");
		return;
	}
	if (delete_old_var(c->arguments[1])<0){
		perror("Error: Variable not found!\n");
	}
	return;
}

int delete_old_var(char* var){
	if (var_list==0){
		return -1;
	}
	if (var_list->var==0){
		return -1;
	}
	link1* prev = var_list;
	if (strcmp(prev->var,var)==0){
		var_list=prev->next;
		free(prev->var);
		free(prev->value);
		free(prev);
		return 1;
	}
	link1* curr;
	while (prev->next!=0){
		curr=prev->next;
		if (strcmp(curr->var,var)==0){
			free(curr->var);
			free(curr->value);
			prev->next=curr->next;
			var_list=prev;
			free(curr);
			return 1;
		}
		prev=curr;
	}
	return -1;
}

void set_com(cmdLine* cmdline){
	if (cmdline->argCount!=3){
		perror("Error: illegal structure of set command");
	}
	delete_old_var(cmdline->arguments[1]);

	link1* temp= malloc(sizeof(link1));
	char* newval = malloc(sizeof(char)*strlen(cmdline->arguments[2])+1);
	strcpy(newval,cmdline->arguments[2]);
	char* newvar = malloc(sizeof(char)*strlen(cmdline->arguments[1])+1);
	strcpy(newvar,cmdline->arguments[1]);
	temp->var=newvar;
	temp->value=newval;
	temp->next=NULL;

	var_list = list_append_last(var_list,temp);
}

int replace_args(cmdLine* c){
	int k;
	for (k=0; k < c->argCount; k++){
		if (strlen(c->arguments[k])==0){ /*bad arg =null*/
			perror("Error: illegal argument- null arg");
			return -1;
		}
		if (c->arguments[k][0]=='$'){ /*a variable*/
			char* var_no_$ = (c->arguments[k])+1;
			link1* cur = var_list;
			int flag=1;
			while (flag && cur!=0){
				if (strcmp(cur->var,var_no_$)==0){
					replaceCmdArg(c,k,cur->value);
		    		flag=0;
				}
				cur = cur->next;
			}
			if (flag){
				printf("Variable error: var not found\n");
				return -1;
			}
		}
	}
	return 1;
}