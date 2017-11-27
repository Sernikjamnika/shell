#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

int ifwait = 1, if_pipe = 0;

//Builtin functions
char *builtin_names[] = {
  "exit",
  "cd"
};

//Exits shell
int function_exit(char **args){
    printf("Bye bye \n");
    return 0;
}

//Changes direction
int function_cd(char **args){
    if (args[1] == NULL) {
        fprintf(stderr, "no arguments");
    } 
    else {
        if (chdir(args[1]) != 0) {
            perror("error");
        }
    }
    return 1;
}

//Array of builtin functions
int (*builtin_func[]) (char **) = {
  &function_exit,
  &function_cd
};

//Reads line without any formatting
//Returns line = stirng 
char *read_line(){
    int bufsize = 1024;
    char *buffer = malloc(sizeof(char) * bufsize);
    int iterator = 0;
    char c;
    while(1){
        c = getchar();
        //kind of cheat to use here but closes shell if EOF 
        if (c == EOF) {
            printf("Bye bye\n");
            exit(1);
        }
	//return array with commands if got newline
        if(c == '\n'){
            buffer[iterator] = '\0';
            return buffer;
        }
	//add new letter to array
        else{
            buffer[iterator] = c;
            iterator++;
        }
	//reallocate if needed
        if(iterator >= bufsize){
            bufsize += 64;
            buffer = realloc(buffer, bufsize);
            // if memory couldn't be alloced
            if(!buffer){
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

//Splites line into commands, delimeter = space
//Returns split = array of strings
char **split(char *line){
    int bufsize_splited = 1024;
    int bufsize_command = 1024;
    int iterator_line = 0;
    int iterator_command = 0;
    int iterator_splited = 0;
    char **splited = malloc(bufsize_splited * sizeof(char*)); // array of words
    char *command = malloc(bufsize_command * sizeof(char)); //one word
    char letter = line[iterator_line];
    while(letter != '\0'){
        //adds word to array of words
        if(letter == ' '){
            splited[iterator_splited] = command;
            iterator_splited++;
            iterator_command = 0;
            bufsize_command = 1024;
            command = malloc(bufsize_command);
            //gets rid of unnecessary spaces
            while(line[iterator_line + 1] == ' '){
                iterator_line++;
                letter = line[iterator_line];
            }

        }
        //adds letter to word
        //if used to catch letter after getting rid of unncessary spaces
        else if(letter != ' '){
            command[iterator_command] = letter;
            iterator_command++;
        }
        iterator_line++;
        letter = line[iterator_line];

        //checks if word not run out of memory if neede reallocates
        if(iterator_command >= bufsize_command){
            bufsize_command += 64;
            command = realloc(command, bufsize_command);
            // if memory couldn't be alloced
            if(!command){
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        //checks if array of words not run out of memory if neede reallocates
        if(iterator_splited >= bufsize_splited){
            bufsize_splited += 64;
            splited = realloc(splited, bufsize_splited * sizeof(char *));
            // if memory couldn't be alloced
            if(!splited){
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    //adds last word if not empty
    if(strcmp(command, "") != 0){
        splited[iterator_splited] = command;
        iterator_splited++;
    }
    //adds NULL at the end, needed for exec func
    splited[iterator_splited] = NULL;
    return splited;
}

//checks if first command is not a built in
//return 1 if empty or builtin
//return 2 if command is not a builtin
int execute_builtin(char **args){
    //checks if NULL
    if (args[0] == NULL) {
        return 1;
    }
    //checks builtin commands
    int length = sizeof(builtin_names) / sizeof(builtin_names[0]), i, counter = 0;
    for(i = 0; i < length; i++){
        if(strcmp(args[0], builtin_names[i]) == 0){
            return (*builtin_func[i])(args);
        }
    }
    return 2;
}

//executes proccess if wasn't builtin
//gets array of strings - commands
int execute(char **commands){
    int bufsize_process = 1024;
    int bufsize_redirect = 1024;
    int iterator_commands = 0;
    int iterator_process = 0;
    int ended_process = 0;
    char ** process = malloc(bufsize_process * sizeof(char*));
    char *redirect = malloc(bufsize_redirect * sizeof(char));
    int i = 0;
    //flags
    int waiting = 1, pipeing = 0, redirecting = 0, reading = 0, error_redirect = 0;; 
    //for processes
    pid_t pid, wpid;
    int status;
    //for pipes
    int p[2];
    int  fd_in = 0;
    //files have to exist because of laziness
    //should have been created if they don't exit
    //should be renamed into redirect, output, error
    int file, file1, file2;
    while(commands[iterator_commands] != NULL){
        // shows that parent will we have to wait
        if(strcmp(commands[iterator_commands], "&") == 0){
            ended_process = 1;
            waiting = 0;
            process[iterator_process] = NULL;
        }
        // //shows that there is a pipe
        else if(strcmp(commands[iterator_commands],"|") == 0){
            ended_process = 1;
            pipeing = 1;
            process[iterator_process] = NULL;
        }
        //redirects output to the file
        else if(strcmp(commands[iterator_commands],">") == 0){
            if(commands[iterator_commands + 2] == NULL){
                ended_process = 1;
            }
            redirecting = 1;
            process[iterator_process] = NULL;
            iterator_commands++;
            redirect = commands[iterator_commands];
            truncate(redirect, 0);
            file = open(redirect, O_WRONLY);
        }
        //treats file as an input
        else if(strcmp(commands[iterator_commands],"<") == 0){
            if(commands[iterator_commands + 2] == NULL){
                ended_process = 1;
            }
            reading = 1;
            process[iterator_process] = NULL;
            iterator_commands++;
            redirect = commands[iterator_commands];
            file1 = open(redirect, O_RDONLY);
        }
        //redirect error message to the file
        else if(strcmp(commands[iterator_commands],"2>") == 0){
            if(commands[iterator_commands + 2] == NULL){
                ended_process = 1;
            }
            error_redirect = 1;
            process[iterator_process] = NULL;
            iterator_commands++;
            redirect = commands[iterator_commands];
            truncate(redirect, 0);
            file2 = open(redirect, O_WRONLY);
        }
        //shows end of commands
        else if(commands[iterator_commands + 1] == NULL){
            process[iterator_process] = commands[iterator_commands];
            iterator_process++;
            ended_process = 1;
            process[iterator_process] = NULL;
        }
        // adds commands to current process
        else {
            process[iterator_process] = commands[iterator_commands];
            iterator_process++;
        }

        //executes process
        if(ended_process){
            pipe(p);
            if ((pid = fork()) == -1){
                exit(EXIT_FAILURE);
            }
            else if (pid == 0){
                if(reading == 0){
                    dup2(fd_in, 0);
                }
                else{
                    dup2(file1, 0);
                }
                 //change the input according to the old one 
                if (commands[iterator_commands + 1] != NULL)
                    dup2(p[1], 1);
                //redirects to file if needed
                else if (redirecting == 1){
                    dup2(file, 1);
                }
                close(p[0]);
                if(error_redirect == 1){
                    dup2(file2, 2);
                }
                execvp(process[0], process);
                exit(EXIT_FAILURE);
            }
            else{
                //if we are wiating for child
                if(waiting == 1){
                    do {
                        wpid = waitpid(pid, &status, WUNTRACED);    
                        //WE'RE WAITING FOR COMMAND TO FINISH RUNNING
                        //waitpit() WAITS FOR PROCESS STATE TO CHANGE
                    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
                }
                //shows pid of child on which we are not waiting
                if(redirecting == 1){
                    close(file);
                } if (reading == 1){
                    close(file1);
                } 
                if(error_redirect == 1){
                    close(file2);
                }
                else if(waiting == 0){
                    printf("%i\n", pid);
                }
                close(p[1]);
                
                fd_in = p[0]; //save the input for the next command
            }
            //clean for new process
            process = malloc(bufsize_process * sizeof(char*));
            iterator_process = 0;
        }
        iterator_commands++;
        ended_process = 0;
    }
    return 1;
}


void infinite_loop(){
    int status = 1;
    char *line;
    char **commands;
    char direction[1024];
    do{
        status = 0;
        getcwd(direction, sizeof(direction));
        printf("%s$ ", direction);
        line = read_line();
        commands = split(line);
        status = execute_builtin(commands);
        if(status == 2){
            status = execute(commands);
        }
        free(commands);
        free(line);

    }while(status);
}

//CTRL-C HANDLERS
void intHandler(int state) {
    fprintf(stderr, "\n\e[1m\x1B[31mCtrl-D to exit this time\x1B[0m\e[0m\n ");
    infinite_loop();
}

int main(void){
    signal(SIGINT, intHandler);
    infinite_loop();
    return 0;
}
