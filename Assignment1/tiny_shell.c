#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>

struct node
{
    char *command;
    struct node *next;
    struct node *prev;
}*head,*tail;
static int history_count=0;    
int signal_is_read=0;


char *get_a_line()
{
	char *line = NULL;
	ssize_t buffer = 0;
	getline(&line, &buffer, stdin);
    line[strlen(line)-1]='\0';
	return line;
}

int process_line(char *line, char **argv)
{
    int cur_idx=0;
    int pipe_idx;
    bool is_pipe=false;
    if (line[0]=='\0' || line[0]=='\n')
        return 0;// according to the instructions, if just the new line char or char is \0, end 
    
    if (history_count==100)//save line to history
    {
        head=head->next;
        free(head->prev);
        tail->next=(struct node *)malloc(1*sizeof(struct node));
        tail->next->prev=tail;
        tail=tail->next;
        tail->command=strdup(line);
    }
    else
    {
        history_count++;
        tail->next=(struct node *)malloc(1*sizeof(struct node));
        tail->next->prev=tail;
        tail=tail->next;
        tail->command=strdup(line);
    }

    while (*line != '\0')
    {  
        while (*line == ' ' || *line == '\t')
        {
            *line='\0';
            line++;// set EOF
        }
        if (*line == '|')
        {
            pipe_idx=cur_idx;
            is_pipe=true;
        }
        *argv = line; //save the argument position
        argv++;
        cur_idx++;
        while (*line != '\0' && *line != ' ' && *line != '\t') 
            line++; //skip
    }
    *argv = '\0'; //set EOF
    if (is_pipe) return pipe_idx+2; //make sure return value greater than 1 so it can make the right pipe call rather than regular call
    return 1;
}

void my_system(char **argv)
{
    if (strcmp(*argv,"history")==0)//if history command
    {
        struct node *cur=head->next->next;
        for(int i=history_count;i!=0;i--)
        {
            printf("  %d  ",history_count-i+1);
            printf(cur->command);
            printf("\n");
            fflush(stdout);
            cur=cur->next;
        }
    }
    else if(strcmp(*argv,"chdir")==0 || strcmp(*argv,"cd")==0)//if cd command
    {
        char s[100]; 
        if (argv[1] == NULL)
            chdir("/home"); // if no command, cd to home
        else
            chdir(argv[1]); 
    }
    else if(strcmp(*argv,"limit")==0)// if limit command
    {
        struct rlimit cur, new;
        getrlimit(RLIMIT_STACK, &cur);
        printf("Current limit: %ld \n", cur.rlim_max);
        fflush(stdout);
        new.rlim_max = atoi(argv[1]);
        new.rlim_cur = cur.rlim_cur;
        setrlimit(RLIMIT_STACK, &new);
        printf("New limit: %ld \n",new.rlim_max);
        fflush(stdout);

    }
    else //if other commands, fork()
    {
        pid_t pid=fork();   
        if (pid < 0) 
        {     
            printf("fork failed\n");
            exit(1);
        }
        else if (pid == 0) //if child
        {
            if (execvp(*argv, argv) < 0) 
            {   
                perror("execvp failed");
                exit(1);
            }
        }
        else //if parent                             
            wait(NULL);
    }
}


void my_system_pipe(char **arguments, int pipe_idx)
/*
reference: https://stackoverflow.com/questions/13801175/classic-c-using-pipes-in-execvp-function-stdin-and-stdout-redirection
*/
{
    arguments[pipe_idx]=0; //change the structure of the arguments, so arguments=[a,b,c,0,d,e,f,....0] where a,b,c are used in the first fork to execute
                           // d,e,f,,, are used in the second fork
    int des_p[2];
    if(pipe(des_p) == -1) {
        perror("Pipe failed");
        exit(1);
    }

    if(fork() == 0)            //first fork
    {
        close(STDOUT_FILENO);  //closing stdout
        dup(des_p[1]);         //replacing stdout with pipe write 
        close(des_p[0]);       //closing pipe read
        close(des_p[1]);
        execvp(arguments[0], arguments);
        perror("execvp failed");
        exit(1);
    }

    if(fork() == 0)            //creating 2nd child
    {
        close(STDIN_FILENO);   //closing stdin
        dup(des_p[0]);         //replacing stdin with pipe read, i.e. pipe read are used as input for the second execvp()
        close(des_p[1]);       //closing pipe write
        close(des_p[0]);
        execvp(arguments[pipe_idx+1], &arguments[pipe_idx+1]);
        perror("execvp failed");
        exit(1);
    }

    close(des_p[0]);
    close(des_p[1]);
    wait(0);
    wait(0);//waiting for the 2 children to complete
    return;
}
void handler(int sig)
{
	if(sig == 2)
    {
		printf("\nQuit? (Y/n)\n");
        fflush(stdout);
		signal_is_read = 1;
	}
}




void main()
{
    char *line;
    char *arguments[100];
    signal(SIGINT, handler);
	signal(SIGTSTP, handler);// setup signal handler
    head=(struct node *)malloc(1*sizeof(struct node)); //initialize history node
    tail=(struct node *)malloc(1*sizeof(struct node));
    head->next=tail;
    tail->prev=head;
    int flag=1; //indicating if shell should run and which mode to use(regular or fifo). 0:No. 1:Yes, regular. Other:Yes, fifo
    
    while (1) 
    {
        line=get_a_line();  
        usleep(20000); 
        if (signal_is_read)
            if (strcmp(line, "Y")==0 || strcmp(line, "y")==0)
                return; 
            else 
                line=get_a_line();
        signal_is_read=0;
        flag=process_line(line, arguments);
        if(flag==0) 
            return;
        else if (flag==1) 
            my_system(arguments);
        else 
            my_system_pipe(arguments,flag-2); //substract the index by 2 so it points to the right position of the pipe character
    }
}