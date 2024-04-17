#include <ctype.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>

//Declare array of pointers to strings for PATH and HIST
char** PATH =NULL;
char** HIST = NULL;
int histcount = 0; //Number of comands entered
int histsize = 20;
int pathsize = 1;
int special = 0; //Coutn of special chars
//Prototypes
void Eval(char* toke, char* sep, int count, char* buffer, int special);
void path(char* toke, char* sep, int count);
void pwd();
void cd(char* toke, char*sep, int count);
void history();
char* findFun(char* toke, char* sep, int count);
void launchFun(char** function,int* argnums, int numfork);
void error();
int savein;
int saveout;
int main(int argc, char* argv[])
{
    savein = dup(0);
    saveout = dup(1);
    //Allocate first spot for /bin\0
    PATH = malloc(sizeof(char*));
    PATH[0] = malloc(sizeof(char)*5);
    strcpy(PATH[0],"/bin");   

    //Allocate pointers
    HIST = malloc(sizeof(char*)*20);
    //Loop through HIST and set pointers to NULL
    for(int i=0; i<histsize; i++)
    {
        HIST[i]=NULL;
    }

    //Check if there are too many args
    if(argc >2)
    {
        error();
        return 0;
    }
    //Bits to be used for quiting and batch mode
    int quit = 0;
    int batch = 0;
    //Check for batch mode and set input handler
    dup2(savein, STDIN_FILENO);
    dup2(saveout,STDOUT_FILENO);
    //Set batch file
    if(argc ==2)
    {
        batch =1;
        int input;
        dup2( input = open(argv[1], O_RDONLY) ,STDIN_FILENO);
    }
    //loop to run shell
    while (!quit)
    {   
        char *buffer = NULL;
        size_t bufsize =32;
        size_t chars;
        //Allocat memory and check if it failed
        buffer = (char *)malloc(bufsize* sizeof(char));
        if(buffer==NULL)
        {
            perror("Unable to allocate memory");
            return(1);
        }
        if(batch==0)
        {
            printf("gush> ");
            dup2(savein, STDIN_FILENO);
        }
        dup2(saveout,STDOUT_FILENO);
        
        chars = getline(&buffer, &bufsize, stdin);

        //Erase the new line char from hitting enter in terminal.
        if(buffer[0]=='\n')
            continue;

        //Launch an old command
        if(buffer[0]=='!')
        {
            //if entered more than 2 digit number
            if(strlen(buffer)>4)
            {
                perror("Number too long \n");
                continue;
            }
            //int to see if History is full or not
            int full=0;
            //if the last item is no longer null, the history is full
            if(HIST[19]!=NULL)
                full=1;

            //single digit number
            int number = 0;
            if(strlen(buffer)==3)
            {
                number = (int)(buffer[1]-'0');
            }
            else //double digit
            {
                number =  10* (int)(buffer[1]-'0') +(int)(buffer[2]-'0');
            }
            if(full)
            {
                free(buffer);
                buffer = (char*)malloc(strlen(HIST[histcount-1+number]));
                strcpy(buffer, HIST[histcount-1+number]);
            }
            else //not full case
            {
                if(number>histcount)
                {
                    perror("Entered number too large \n");
                    continue;
                }
                free(buffer);
                buffer = (char*)malloc(strlen(HIST[number-1]));
                strcpy(buffer, HIST[number-1]);
            }
            
        }
        //Remove the newline for clean printing and processing
        if(buffer[strlen(buffer)-1]=='\n')
            buffer[strlen(buffer)-1] = 0;

        //Loop through input to convert it to normal form with spaces
        int numspace = 0; //Var to count the number of spaces added, used for reaaloc
        char * copy = (char*)malloc(strlen(buffer)+1); //Copy to be transformed from original buffer
        int count = 0; //Count of number of arguments
        int off = 0; //offset to be used when adding spaces
        special = 0; //Count of special characters 
        for(int i=0; i<strlen(buffer); i++)
        {
            if(buffer[i]=='<' ||buffer[i]=='>' ||buffer[i]=='&' ||buffer[i]=='|')
            {
                copy = (char*)realloc(copy, (strlen(buffer)+numspace+2)*sizeof(char));
                //Put in space before and after char, use off to count number of added chars
                copy[i+off] = ' ';
                copy[i+off+1] = buffer[i];
                copy[i+off+2] = ' ';
                numspace+=2;
                off+=2;
                count+=3;
                special+=1;
            }
            else
            {
                copy[i+off] = buffer[i];
                count+=1;

            }
        }
        //  ps -aux|grep sbin|nl
        //Looking for 0-23
        if(count!=0)
        {
            copy = (char*)realloc(copy, (count)*sizeof(char));
            copy[count]='\0';
            count=0;
        }
        char *sep = " ";
        free(buffer);
        buffer = (char*)malloc(strlen(copy)+1);
        strcpy(buffer, copy);
        char *toke = strtok(copy, sep);
        while(toke!=NULL)
        {
            count ++;
            if(!strcmp(toke,"<"))
            {
                int input;
                toke = strtok(NULL, sep);
                savein = dup(STDIN_FILENO);
                dup2(input = open(toke, O_RDONLY),STDIN_FILENO);
                count++;
            }
            if(!strcmp(toke,">"))
            {
                
                int output;
                int err;
                toke = strtok(NULL, sep);
                dup2(output = open(toke, O_RDWR||O_CREAT),STDOUT_FILENO);
                dup2(err =open(toke, O_RDWR||O_CREAT),STDERR_FILENO);
                count++;
            }
            toke = strtok(NULL, sep);
        }
        free(copy);
        copy = (char*)malloc(strlen(buffer)+1);
        strcpy(copy,buffer);
        toke = strtok(buffer, sep);
        Eval(toke, sep, count, copy, special); 
        free(buffer); 
    }
    
    return 0;
}

void Eval(char* toke, char* sep, int count, char* buffer, int special)
{ 
    //May need to be changed when reading from actual file
    if(  (toke==NULL && strlen(buffer)==0) )
    {
        printf("Exiting\n");
        exit(0);
    }
    //Check if character is null to avoid crash on strcmp
    //This would happen if user input spaces and hit enter
    if(toke==NULL)
        return;

    //Now check if input is exit
    if(!strcmp(toke,"exit"))
    {
        printf("Exiting\n");
        exit(0);
    }

    if(!strcmp(toke,"history"))
    {
        if(count!=1)
            perror("History takes no arguments \n");
        else
           history();
        return;
    }
    //Store command in HISTORY
    HIST[histcount] = malloc(sizeof(char)*strlen(buffer));
    strcpy(HIST[histcount],buffer);
    histcount = (histcount+1)%histsize;

    if(!strcmp(toke,"kill"))
    {
        toke = strtok(NULL,sep);
        if(toke==NULL)
        {
            perror("You done messed up \n");
        }
        else
        {
            int pid = atoi(toke);
            if(pid==0)
                perror("invalid pid \n");
            kill(pid,SIGKILL);
        }
        
    }
    if(!strcmp(toke,"path"))
    {
        //Advance toke
        toke = strtok(NULL, sep);
        count--;
        path(toke, sep, count);
        return;
    }
    else if(!strcmp(toke,"pwd"))
    {
        pwd();
    }
    else if(!strcmp(toke, "cd"))
    {
        //Advance toke
        toke = strtok(NULL, sep);
        count--;
        cd(toke, sep, count);
        return;
    }
    else
    {
        if(special==0)//normal case
        {
            ///use find fun to searh PATH
            char* program = findFun(toke, sep, count);
            count--;
            toke = strtok(NULL, sep);
            if(program ==NULL)
            {
                error();
                return;
            }  
            //Create arglist to be used for launch 
            char** argList = malloc(sizeof(char*)*(count+1));
            argList[0] = malloc(sizeof(char)*(strlen(program)+1));
            strcpy(argList[0], program);
            //iterate through extra params and build array
            for(int i =0; i<count; i++)
            {
                argList[i+1] = malloc( (strlen(toke)+1)*sizeof(char));
                strcpy(argList[i+1], toke);
                toke = strtok(NULL,sep);
            }
            //Append NULL to arglist
            argList[count+1] = malloc(sizeof(char));
            argList[count+1] =NULL;     
            launchFun(argList, NULL, 0);
            //for(int i=0; i<count+1; i++)  
                //free(argList[i]);
            //free(argList);
        }//If command contains no special chars
        else
        {   
            char** function = malloc(sizeof(char*) * count);
            for(int i=0; i<count; i++)
                function[i]=NULL;
            int funcsize = count;
            //Iterate through args until special char
            int i = 0;
            int search = 1; //bit to indicate if we should serach for exe
            int numargs = 0; //Count to get the size of final array
            int argsize [1000];
            int paramnum = 0; //Counter to be used for params
            int numfork = 0; //Counter to determine number of forks needed
            while(toke!=NULL)
            {
                if(search ==1)
                {
                    char* program = findFun(toke, sep, count);
                    count--;
                    toke = strtok(NULL, sep);
                    count--;
                    if(program ==NULL)
                    {
                        error();
                        return;
                    }
                    function[i] =(char*) malloc((strlen(program)+1)*sizeof(char));
                    strcpy(function[i],program);
                    paramnum++;
                    i++;
                }
                else
                {
                    function[i] = (char*)malloc((strlen(toke)+1)*sizeof(char));
                    strcpy(function[i], toke);
                    count--;
                    toke = strtok(NULL, sep);
                    i++;
                }
                if(toke!=NULL)
                {
                    while(strcmp(toke,"|") && strcmp(toke,">") &&strcmp(toke,"<") &&strcmp(toke,"&"))
                    {
                        function[i] = malloc( (strlen(toke) +1) * sizeof(char));
                        strcpy(function[i],toke);
                        toke = strtok(NULL, sep);
                        count--;
                        i++;
                        paramnum++;
                        if(toke==NULL)
                            break;
                    }
                }
                //Put arg number into array if normal char
                if(search==1)
                {
                    argsize[numfork] = paramnum;
                    numargs++;
                    paramnum=0;
                    numfork++;
                }
                //Copy special char and set search bit
                if(toke!=NULL)
                {
                    function[i] = malloc(sizeof(char)*2);
                    strcpy(function[i],toke);
                    i++;
                    //This setup ensures if we have double redirection they
                    //Wont be counted as arguments
                    if(!strcmp(toke, "<") ||!strcmp(toke, ">"))
                        search = 0;
                    else
                        search = 1;        
                }               
                if(toke!=NULL)
                    toke =strtok(NULL,sep);
            }//While toke!= NULL

            //Put a null at the end of the buffer
            function[i]=malloc(sizeof(char));
            function[i]= "\0";

            //Get the number of params for last command
            int* args = malloc(sizeof(int)*numargs);
            for(int i =0; i<numargs;i++)
            {
                args[i]=argsize[i];
                //free(argsize[i]);
            }
            launchFun(function, args, special);

            //This is def messed up rn.
            //for(int i=0; i<funcsize;i++)
                //free(function[i]);
            //free(function);
            //free(args);

        }// else for special chars loops  
    }//Launch a command else
    
}//Eval loop

void history()
{
    //Want to go from my counter to counter -1 and check if 
    //pointer is null
    //set offset to be next available space
    int offset = histcount;
    int command = 1;
    //This will run 20 times
    for(int i= 0; i< histsize; i++)\
    {
        if(HIST[offset]!=NULL)
        {
            printf("%d: %s \n", command, HIST[offset]);
            command++;
        }
        offset= (offset+1)%histsize;
    }

}
//Assume initial argument cd has been read
void cd(char* toke, char*sep, int count)
{
    //Check if we should go home
    if(count==0)
    {
        if(chdir("/home")!=0)
            error();
    }
    else if(count==1)
    {
        if(chdir(toke)!=0)
            error();
    }
    else
    {
        error();
    }
}


//pwd call
void pwd()
{
    char* cwd = getcwd(NULL, 0);
    if(cwd!=NULL)
    {
        printf("%s \n", cwd);
    }
    else
    {
        error();
    } 
}

//Assuming the initial argument path has been read
void path(char* toke, char* sep, int count)
{
    //If called with no params, delete path
    if(count ==0)
    {
        for(int i = 0; i<pathsize;i++)
        {
            free(PATH[i]);
        }
        PATH = NULL;
        pathsize=0;
    }
    else
    {
        int offset = 0;
        for(int i = 0; i<pathsize;i++)
        {
            free(PATH[i]);
        }
        free(PATH);
        //Loop through params and allocate space
        PATH = malloc(count*sizeof(char*));
        pathsize=count;
        while(count !=0)
        {
            PATH[offset] = malloc(sizeof(char)*(strlen(toke)+1));
            strcpy(PATH[offset],toke);
            count--;
            offset++;
            toke = strtok(NULL, sep);
        }
    }
    
}

char* findFun(char* toke, char* sep, int count)
{
    char* program = NULL;
    if(access(toke,X_OK)==0)
    {
        return toke;
    }
    else 
    {
        for(int i=0; i<pathsize; i++)
        {
            //Setup querry string
            int size = strlen(PATH[i]) + strlen(toke)+3;
            char* querry = malloc(size);
            strncpy(querry, PATH[i], strlen(PATH[i]));
            querry[strlen(PATH[i])] = '/';
            strcat(querry, toke);
           
            //Need to add access of just toke in case they specify absolute path
            //If the querry is good copy it over for execution
            if(access(querry, X_OK)==0)
            {
                program = (char*)malloc(strlen(querry)+1);
                strcpy(program, querry);
                //free(querry);
                return program;
            }
            //free(querry);                  
        }
        return NULL;
    }//End of else for testing paths
}//End of findFun method

//Assume funciton is a null temrinated array containing the function
void launchFun(char** function, int* argnums, int numfork)
{
    int status;
    
    if(numfork==0)
    {
        pid_t pid = fork();

        if( pid==0) //Child
        {
            execve(function[0], function, NULL);
            exit(0);
        }
        else
        {
            if(waitpid(pid, &status, 0) >0)
                return;
        }
    } //End of normal case
    else
    {        
        int iteration = 0; //count for number of times looped
        int numb=0; //used to synch up pipe usage
        int pp = 0; //Bit used to track if pipe needs to stay open
        int index = 0; //used to move along param array
        int status; //bit used to indicate if doulbe pipe is needed
        int background = 0; //bit used to run in background
        int** pipes = malloc(sizeof(int*)*numfork);
        int change = 0; //bit used to indicate if stdin should be set to pipe
        pid_t parent;
        //Create an array of pipes to be used for potential piping
        for(int i= 0; i<numfork; i++)
        {
            pipes[i] = malloc(2*sizeof(int));
            pipe(pipes[i]);
        }
        while(strcmp(function[index], "\0"))
        {
            pid_t pid = fork();

            if(pid==0)//Child
            {
                int argcap = argnums[iteration]; //number of args for this function
                char**launch = malloc(sizeof(char*)*argcap);
                
                for(int i =0; i<argcap; i++)
                {
                    launch [i] = malloc(sizeof(char)* (strlen(function[index])+1));
                    strcpy(launch[i],function[index]);
                    index++;
                }
                launch[argcap] = malloc(sizeof(char));
                launch[argcap] = NULL;

                //If the special char is redirect case
                while(!strcmp(function[index],"<") ||!strcmp(function[index],">"))
                {
                    if( !strcmp(function[index],"<"))
                    {
                        int input;
                        dup2(input = open(function[index+1], O_RDONLY),STDIN_FILENO);
                        index+=2;
                    }
                    if( !strcmp(function[index],">"))
                    {
                        int output;
                        int err;
                        dup2(output = open(function[index+1], O_RDWR||O_CREAT),STDOUT_FILENO);
                        dup2(err = open(function[index+1], O_RDWR||O_CREAT),STDERR_FILENO);
                        index+=2;
                    }
                }
                if(!strcmp(function[index],"|")&&change==0)
                {
                    pp=1;
                    dup2(pipes[numb][1], STDOUT_FILENO); //set std out to write
                    close(pipes[numb][0]);           
                }
                else
                    pp=0;              
                if(change==1)
                {
                    dup2(pipes[numb][0], STDIN_FILENO); //set stdin 
                    close(pipes[numb][1]);
                    numb++;
                    if(!strcmp(function[index],"|"))
                    {
                        pp=1;
                        dup2(pipes[numb][1], STDOUT_FILENO);
                        close(pipes[numb][0]);
                    } 
                    else
                        pp=0;
                                   
                }
                
                execve(launch[0], launch, NULL);
                //Could launch here?
            }//Child if
            else //parent
            {
                close(pipes[numb][1]);
                if(pp==1)
                    close(pipes[numb][0]);
                index = index + argnums[iteration];
                if(strcmp(function[index],"&"))
                    waitpid(pid,&status,0);
                
                else
                    background=1;
                
                while(!strcmp(function[index],"<") ||!strcmp(function[index],">"))
                {
                    index+=2;
                }
                if(change==1)
                    numb++;
                if(!strcmp(function[index],"|"))
                    change=1;
                else
                    change=0;
                
                //If we're not at the end
                if(strcmp(function[index],"\0"))
                    index++;

                iteration++;
            }

        } //While function[index] != NULL

        if(background==1)
            wait(NULL);

    }//End of special case  
}

void error()
{
    char mess[30]="An error has occurred\n";
    write(STDERR_FILENO, mess, strlen(mess));
    return;
}