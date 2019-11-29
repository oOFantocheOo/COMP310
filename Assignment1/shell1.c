#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/stat.h>
#include<sys/resource.h>
#include<signal.h>

char *get_a_line();				
int my_system(char* line);		
int my_chdir(char **args);		
int my_history(char **args);	
int my_limit(char **args);		
int my_stop(char **args);		
void my_handler(int sig);		//function declare
char *history[100];				//used to store historical commands
int current = 0;				//point to current command
int EofF = 0;					//if 1 then EOF
int keep_running = 1;			//check if Ctrl c been pressed

char *builtin_string[] = {
	"chdir",
	"cd",
	"history",
	"limit"
};								//record the buildin commands

int (*builtin_function[])(char **) = {
	&my_chdir,
	&my_chdir,
	&my_history,
	&my_limit
};								//record the buildin functions

int number_of_functions(){
	return sizeof(builtin_string)/sizeof(char*);
}								//count the number of buildin commands

int main(){
	char* line;						//buffer
	signal(SIGINT, my_handler);		//handle Ctrl c
	signal(SIGTSTP, my_handler);	//handle Ctrl z
	printf("tiny_shell> ");			//prompt
	line = get_a_line();			//get a line
	while(EofF != -1) {				//while not EOF
		if(EofF != -1 &&((strcmp(line, "Y\n")!=0 && strcmp(line, "y\n")!=0) || keep_running)){
		//while not EOF; when Ctrl c been pressed, if the following input is not Y or y, continue the loop
			if(keep_running == 0){	//Ctrl c been pressed but following input is not Y or y
				keep_running = 1;	//reset keep_running
			}
			else if(strlen(line) > 1){
				my_system(line);	//if there's command, run it
			}
		}
		else{
			printf("*** NORMAL EXIT");
			break;					//exit by Ctrl c
		}
		printf("tiny_shell> ");		
		line = get_a_line();		//next
	}
	if(EofF == -1)printf("*** End of File");	//print if exit by EOF
}

char *get_a_line(){
	char *line = NULL;
	ssize_t buf = 0;
	EofF = getline(&line, &buf, stdin);
	return line;
}									//use getline to get input


int my_system(char *args){
	char *args_t = strdup(args);					
	args_t[strlen(args_t)-1] = '\0';				
	history[current] = args_t;
	current = (current + 1)%100;					//duplicate the input and store in history, update current

	char **input = malloc(64*sizeof(char*));		
	char* temp = strtok(args, "\t\r\n\a ");
	int c = 0;
	while(temp != NULL){
		input[c] = temp;
		c++;
		temp = strtok(NULL, "\t\r\n\a ");
	}												//split input by "\t\r\n\a " to an String Array

	int i,j,k,pid, wpid,status,fd;
	FILE *fp;
	char *file = " ";
	char **input_sup = malloc(64*sizeof(char*));	//create supplement input array for pipe
	
	if(input[0] == NULL){
		return 1;
	}												//return if no input
	
	k = 1;											//k = 1 if no '|' in argument

	for(i = 0; i < c; i++){
		if(!strcmp(input[i],"|")){
			k = 0;
			break;	
		}
	}												//check if '|' in argument

	for(j = 0; j < c-i-1; j++){
		input_sup[j] = strdup(input[i+1+j]);
	}												//duplicate rest of array after '|' to input_sup
	

	for(i; i < c; i++){
		input[i] = NULL;
	}												//clear substance include and after '|' in original input array

	for(i = 0; i < number_of_functions(); i++){
		if(strcmp(input[0], builtin_string[i]) == 0){
			return(*builtin_function[i])(input);
		}
	}												//check if buildin function been called
	
	char *fifoPath = "/tmp/fifo";
	mkfifo(fifoPath,0777);							//create fifo
	char buffer[4096];
	memset(buffer, '\0', sizeof(buffer));			//initilize buffer
	
	pid = fork();
	if(pid == 0){									//child process
		fd = open(fifoPath, O_WRONLY);				
		dup2(fd,1);									//replace stdout by fifo
		if(execvp(input[0],input) < 0){
			printf("*** ERROR: EXECUTION FAILED\n");
			close(fd);
			exit(1);
		}											//execute the command, exit if failed
	}
	else{											//parent process
		wpid = waitpid(0,&status,WNOHANG|WUNTRACED);//wait child
		fd = open(fifoPath, O_RDONLY);
		read(fd, buffer, sizeof(buffer));			//read from fifo
		if(k){										//if no pipe, print and quit
			printf("%s",buffer);
			close(fd);
		}
		else{										
			if(fork() == 0){						//child process
				fp = fopen(file,"w+");				//open a temp file
				if(fp != NULL){
					fputs(buffer,fp);
					fclose(fp);
				}									//print buffer to the file
				input_sup[j] = strdup(file);		//add the file as last argument
				if(execvp(input_sup[0], input_sup)<0){
					printf("*** ERROR: EXECUTION FAILED\n");
					close(fd);
					exit(1);
				}									//execute the command after '|', exit if failed
			}
			else{									//parent process
				wpid = waitpid(0,&status,WNOHANG|WUNTRACED);
				sleep(1);							//wait for child
				close(fd);
			}
		}
	}
	return 1;
}

int my_chdir(char **args){					//implement chdir and cd
	if (args[1] == NULL){					
		chdir("/home/2019/jwang290");
	}										//chdir to home if no argument
	else{
		if(chdir(args[1]) != 0){
			fprintf(stderr,"return error");
		}									//execute and return error if failed
	}
	return 1;
}

int my_history(char **args){				//implement history			
	int i = current;
        int j = 1;
        do{
                if (history[i]) {
                        printf("%4d  %s\n", j, history[i]);
                        j++;
                }
                i = (i + 1) % 100;
        }while (i != current);				//print all substants in history
        return 1;
}

int my_limit(char **args){
	struct rlimit old,new;

	if(getrlimit(RLIMIT_STACK, &old) != 0){
		fprintf(stderr, "Something wrong(1)\n");
	}
	else{
		printf("Old Soft Limit = %ld \n\t Old Hard Limit = %ld \n", old.rlim_cur, old.rlim_max);
	}

	new.rlim_cur = atoi(args[1]);
	if(args[2] == NULL){
		new.rlim_max = old.rlim_max;
	}
	else{
		new.rlim_max = atoi(args[2]);
	}
	
	if(setrlimit(RLIMIT_STACK, &new) == -1){
	       fprintf(stderr, "Something wrong(2)\n");
	}
	else{
		printf("New Soft Limit = %ld \n\t New Hard Limit = %ld \n", new.rlim_cur, new.rlim_max);
        }

	return 0;
}

void my_handler(int sig){
	if(sig == 2){
		printf("\nQuit? (Y/N)\n");
		keep_running = 0;
	}
}
