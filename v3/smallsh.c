#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include  <fcntl.h>

//#include <libexplain/fork.h>
#include <errno.h>


void expandVariableInArray(char* arr);
void expandVariables();
void writePIDstring(char* pidString);
void itoa(int n, char s[]);
void reverse(char s[]);
void printExitStatus(pid_t childPID, int childExitMethod);


//GLOBAL VARIABLES

struct commandLine{
    char commandArgs[2048]; //stores string of command plus arg1, arg2, ...
    int isInputRedir; //flag indicates input redirection
    char inputRedir[512]; //input redirection
    int isOutputRedir; //indicates output redirection
    char outputRedir[512]; //output redirection
    int isBackground; //flag indicates & at end of command
};

//stores tokenized command string
struct commandArgs{
    char command[32];  //command only
    char *args[512]; //arguments stored as array
    int numArgs;
}
;

//stores list of PIDs
struct pidList{
    pid_t processes[64];
    int count;
}
;

//struct stores process exit info
struct exit{
    int isExit;
    int exitStatus;
    int isSignal;
    int exitSignal;
}
;

//global variable to store currently inputted command from user
struct commandLine currCommand;
struct commandArgs currArgs;

//global variable list of background processes
struct pidList backgroundPID;

//global variable string storing initial working directory
char initCD[128];

//global variable stores exit status of last completed foreground process
struct exit lastExit;

//global variable, stores 1/0 flag indicating whether background commands available/unavailable
int backgroundAvailable;

//this function initializes values and strings in the struct
void initializeCurrCommand(){
    memset(currCommand.commandArgs,'\0',sizeof(currCommand.commandArgs));
    memset(currCommand.inputRedir,'\0',sizeof(currCommand.inputRedir));
    memset(currCommand.outputRedir,'\0',sizeof(currCommand.outputRedir));
    currCommand.isInputRedir=0;
    currCommand.isOutputRedir=0;
    currCommand.isBackground=0;
}

//function for testing
//prints out contents of command struct
void printCommandPieces(){
    printf("commandargs: %s\n", currCommand.commandArgs);
    printf("input redirect: %s\n", currCommand.inputRedir);
    printf("output redirect: %s\n", currCommand.outputRedir);
    
    printf("is input redirect: %i\n", currCommand.isInputRedir);
    printf("is output redirect: %i\n", currCommand.isOutputRedir);
    
    printf("is background: %i\n", currCommand.isBackground);
    
    return;
}

//function for testing
void printCommandArgs(){
    int i;
    printf("command %s\n", currArgs.command);
    printf("numArgs %i\n", currArgs.numArgs);
    for(i=0; i<currArgs.numArgs; i++){
        printf("arg %i %s\n", i, currArgs.args[i]);
    }

}


//function catches SIGSTP in parent process
//toggles background process availability on/off
void catchSIGTSTP(int signo)
{
    char* offMessage = "Entering foreground-only mode (& is now ignored)\n";
    char* onMessage = "Exiting foreground-only mode\n";
    
    if(backgroundAvailable){
        //turn background availability off
        write(STDOUT_FILENO, offMessage, 50);
        backgroundAvailable=0;
    }
    else{
        //turn background availability on
        write(STDOUT_FILENO, onMessage, 29);
        backgroundAvailable=1;
    }

}

//source note: this function is based off of userinput_adv.c sample code
//returns 1 if user line matches pattern of a command
//return 0 if it is blank line or commment (no command to execute)
//performs initial interpretation of command, stores pieces in currCommand struct
int readCommand()
{

    int numCharsEntered = -5; // How many chars we entered
    int currChar = -5; // Tracks where we are when we print out every char
    size_t bufferSize = 0; // Holds how large the allocated buffer is
    char* lineEntered = NULL; // Points to a buffer allocated by getline() that holds our entered string + \n + \0

    int  end, front;
    char *infile, *outfile;

    // Get input from the user
    while(1)
    {
        printf(":");
        fflush(stdout);
        // Get a line from the user
        numCharsEntered = getline(&lineEntered, &bufferSize, stdin);
        if (numCharsEntered == -1)
            clearerr(stdin);
        else
            break; // Exit the loop - we've got input
    }



    /*interpret the raw inputted line, 
        check front to see if executable line, 
        then tokenize from end
    */

    //clear out struct which will store command pieces
    initializeCurrCommand();

    end = strcspn(lineEntered, "\n");

    //skip initial whitespace
    front=0;
    while(lineEntered[front] == ' ' && front < end){
        front++;
    }

    //check for empty line
    if(end == 0 || front == end){
        return 0;
    }

    //check for first character indicating comment
    if(lineEntered[front] == '#'){
        return 0;
    }


    //check for background operator '&' at end
    if(lineEntered[end-1] == '&'){
        currCommand.isBackground=1;
        end--;
    }

    //check for output redirect
    outfile = strchr(lineEntered, '>');
    //if the > character was found
    if(outfile != NULL){
        currCommand.isOutputRedir=1;
        strncpy(currCommand.outputRedir, outfile + 1, ((lineEntered + end) - outfile - 1)); //pointer math to calc length of redir portion
        end = end - ((lineEntered + end) - outfile);  //move end to before output redirection section
    }


    //check for input redirection
    infile = strchr(lineEntered, '<');
    if(infile != NULL){
        currCommand.isInputRedir=1;
        strncpy(currCommand.inputRedir, infile + 1, ((lineEntered + end) - infile -1)); //pointer math to calc length of redir portion
        end = end - ((lineEntered + end) - infile);  //move end to before output redirection section
    }




    //remaining text between front and end is command with arguments

    if(! (front < end)){
        printf("Error interpreting command");
        exit(1);
    }


    strncpy(currCommand.commandArgs,&lineEntered[front], end-front);


    // Free the memory allocated by getline() or else memory leak
    free(lineEntered);
    lineEntered = NULL;

    return 1;
}

//wrapper function -- makes calls to expandVariableInArray for all three command strings
void expandVariables(){

    expandVariableInArray(currCommand.commandArgs);
    
    if(currCommand.isInputRedir){
        expandVariableInArray(currCommand.inputRedir);
    }
    
    if(currCommand.isOutputRedir){
        expandVariableInArray(currCommand.outputRedir);
    }
}

//removes leading and trailing whitespace and expands variable "$$" into the pid
//writes cleaned up string into temp, then copies back into source arr
void expandVariableInArray(char* arr){
    char temp[2048];
    int read=0;
    int write=0;
    int pidRead=0;
    char pidString[20];
    char* ptr;

    //clear out temp buffer
    memset(temp, '\0', sizeof(temp));

    //get first non-whitespace
    while(arr[read] == ' '){
        read++;
    }

    //interpret source arr, write into temp
    while(arr[read] != '\0'){
        if(arr[read] == '$' && arr[read+1] == '$'){
            //if detect variable to expand into pid
            
            //get pid, store pid as local variable string
            writePIDstring(pidString); 

            //write pid string to temp buffer
            pidRead=0;
            while(pidString[pidRead] != '\0' && pidRead < 20){
                temp[write] = pidString[pidRead];
                write++;
                pidRead++;
            }

            read = read + 2; //move read past 2 $ charactoers

        }
        else{
            temp[write]=arr[read];
            write++;
            read++;
        }
    }

    //remove any trailing whitespace from string in temp

    ptr = strchr(temp, '\0'); //find end of string
    write = ptr - temp; // convert to index number
    write--; //write now index of character before null terminator

    while(write > 0 && temp[write] == ' '){
        temp[write] = '\0';         //replace trailing empty with null terminator
        write--;
    }



    //updated string now in temp, copy back to original source array
    memset(arr, '\0', sizeof(arr));
    strcpy(arr, temp);


}

//gets pid and writes it to string passed as parameter
void writePIDstring(char* pidString){
    int processID;
    memset(pidString, '\0', sizeof(pidString));

    processID = getpid();   

    itoa(processID,pidString); //convert pid to string
}



//REFERENCE NOTE:  this function found at https://en.wikibooks.org/wiki/C_Programming/stdlib.h/itoa
// which cites Kernighan and Ritchie's The C Programming Language, as original source
/* reverse:  reverse string s in place */
//used by itoa function
void reverse(char s[])
{
    int i, j;
    char c;

    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}
 
//REFERENCE NOTE:  this function found at https://en.wikibooks.org/wiki/C_Programming/stdlib.h/itoa
// which cites Kernighan and Ritchie's The C Programming Language, as original source
 /* itoa:  convert n to characters in s */
 // converts integer to ascii string
void itoa(int n, char s[])
{
    int i, sign;

    if ((sign = n) < 0)  /* record sign */
        n = -n;          /* make n positive */
    i = 0;
    do {       /* generate digits in reverse order */
        s[i++] = n % 10 + '0';   /* get next digit */
    } while ((n /= 10) > 0);     /* delete it */
    if (sign < 0)
        s[i++] = '-';
    s[i] = '\0';
    reverse(s);
}

//takes commandArgs string containing command + arguments
//transforms into array of separated arguments
void tokenizeArgs(){
    int l, r, i;
    l=0;
    r=1;

    //intitialize values in struct
    currArgs.numArgs=0;
    memset(currArgs.command, '\0', sizeof(currArgs.command));

    for(i=0; i<512; i++){
        currArgs.args[i] = NULL;
    }
    


    while(currCommand.commandArgs[r] != '\0'){
        if(currCommand.commandArgs[r] == ' '){
            //right index has blank space, then l to r is a token
            currArgs.args[currArgs.numArgs] = malloc(32*sizeof(char));  //allocate string for token
            strncpy(currArgs.args[currArgs.numArgs], currCommand.commandArgs + l, (r-l)); //copies token into array
            currArgs.numArgs++;

            //adjust indexes, position l on first non-whitespace character, r =l+1
            l=r;
            while(currCommand.commandArgs[l] == ' '){
                l++;
            }
            r=l+1;
        }
        else{
            //right index not a token separator
            r++;
        }
    }

    //get last token after detecting '\0' at right index
    currArgs.args[currArgs.numArgs] = malloc(32*sizeof(char));
    strncpy(currArgs.args[currArgs.numArgs], currCommand.commandArgs + l, (r-l)); //copies token into array
    currArgs.numArgs++; 


    //commandArgs array now has the command at position 0, args starting at position 1
    //copy command itself into command field;
    strcpy(currArgs.command,currArgs.args[0]);

    return;

}



//returns 1 if matches a built-in function name
int isBuiltInFunction(){
    char builtIn[3][16]={"exit", "cd", "status"};
    int i;

    for(i=0; i<3; i++){
        //if command matches a built-in return 1
        if(strcmp(currArgs.command, builtIn[i]) == 0){
            return 1;
        }
    }

    //not match a built-in
    return 0;
}

//runs non-built-in command in child process
//sets up input/output redirection
//makes execvp call
void runChildProcess(){
    int execStatus=0;
    int infile, outfile, result;

    //if run in background, set up default I/O redirection to /dev/null if none given
    if(currCommand.isBackground){
        if(currCommand.isInputRedir == 0){
            currCommand.isInputRedir = 1;
            strcpy(currCommand.inputRedir, "/dev/null");
        }
        if(currCommand.isOutputRedir == 0){
            currCommand.isOutputRedir = 1;
            strcpy(currCommand.outputRedir, "/dev/null");
        }

        //TEST INFO
        //printCommandPieces();
    }


    if(currCommand.isInputRedir){
        //redirect input
        infile = open(currCommand.inputRedir, O_RDONLY);
        if(infile<0){
            printf("Error opening input file: %s\n", currCommand.inputRedir);
            exit(1);
        }
        
        //set input to specified
        result = dup2(infile, 0);
        if(result == -1){
            printf("Error setting input\n");
            exit(1);
        }
    }

    if(currCommand.isOutputRedir){
        //redirect output

        //open output file, create if not exists, 
        outfile = open(currCommand.outputRedir, O_WRONLY | O_CREAT | O_TRUNC,  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);  
        if(outfile < 0){
            printf("Error opening file: %s\n", currCommand.outputRedir);
            exit(1);
        }

        //set output to new file
        result = dup2(outfile, 1);
        if(result == -1){
            printf("Error setting output\n");
            exit(1);
        }
    }


    //set up NULL termination of arguments
    currArgs.args[currArgs.numArgs] = NULL;
    //try to execute command
    execStatus = execvp(currArgs.command, currArgs.args);

    //check for error
    if(execStatus == -1){
        printf("Error: Unable to execute command %s\n",currArgs.command);
    }

    return;
}


//function checks on list of background processes to see if they have completed
void checkBackground(){
    int i,j;
    pid_t childPID_actual;
    int childExitMethod;

    //check each process in list
    for(i=0; i<backgroundPID.count; i++){
        childPID_actual = waitpid(backgroundPID.processes[i], &childExitMethod, WNOHANG);

        //if process i has completed
        if(childPID_actual != 0){
           // printf("Process %d has finished\n",childPID_actual);
            printExitStatus(backgroundPID.processes[i], childExitMethod);

            //clean up array of processes, remove completed
            for(j=i; j<backgroundPID.count; j++){
                backgroundPID.processes[j]=backgroundPID.processes[j+1];
            }
            backgroundPID.count--;
        }

    }


    return;
}


void printExitStatus(pid_t childPID, int childExitMethod){
    int exitStatus, termSignal; //store info on exited child process

    if(WIFEXITED(childExitMethod)){
        exitStatus = WEXITSTATUS(childExitMethod);
        printf("background pid %d is done: exit value %d\n", childPID, childExitMethod);
    }

    if(WIFSIGNALED(childExitMethod)){
        termSignal = WTERMSIG(childExitMethod);
        printf("background pid %d is done: terminated by signal 15 %d\n", childPID, termSignal);
    }
}


void saveExitStatus(int childExitMethod){
    int exitStatus, termSignal; //store info on exited child process

    if(WIFEXITED(childExitMethod)){
        exitStatus = WEXITSTATUS(childExitMethod);
        lastExit.isExit=1;
        lastExit.isSignal=0;
        lastExit.exitStatus=exitStatus;
        //printf("exit status was %d\n", exitStatus);
    }

    if(WIFSIGNALED(childExitMethod)){
        termSignal = WTERMSIG(childExitMethod);
        lastExit.isExit=0;
        lastExit.isSignal=1;
        lastExit.exitSignal=termSignal;
        //printf("exit term status was %d\n", termSignal);
    }
}


//this function runs the built-in cd command
void updateCD(){
    int status_chdir;
    char* envString;

    //if only command without args, set cwd to HOME
    if(currArgs.numArgs == 1){
        envString = getenv("HOME");
        if(envString == NULL){
            printf("Error finding HOME env var\n");
            exit(1);
        }
        status_chdir = chdir(envString);
        if(status_chdir != 0){
            printf("error setting cd\n");
        }        
    }
    //if cd command + 1 argument, set to arg
    else if(currArgs.numArgs == 2){
        status_chdir = chdir(currArgs.args[1]);
        if(status_chdir != 0){
            printf("error setting cd\n");
        }
    }
    else{
        printf("ERROR CD incorrect parameter combination\n");
    }


}

//function kills all running child background processes 
void killAllProcesses(){

    return;
}

int main(){
    int isExecutable; // 0 if comment or empty, 1 if command
    pid_t spawnPID =-5;
    int childExitMethod = -5;
    int i;
    char* status_getcwd;
    int status_chdir;

    //initialize background pid list
    backgroundPID.count=0;

    //store initial working directory
    memset(initCD,'\0',sizeof(initCD));
    status_getcwd = getcwd(initCD, sizeof(initCD));
    if(status_getcwd == NULL){
        printf("Error getting cwd\n");
        exit(1);
    }

    //intitialize last exit status
    lastExit.isExit=1;
    lastExit.isSignal=0;
    lastExit.exitStatus=0;

    //initialize backgroundAvailability
    backgroundAvailable=1;

    //initialize parent signal handlers
    struct sigaction SIGTSTP_action = {0}, ignore_action = {0}, default_action = {0};
    
    //function handles SIGSTP
    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags=0;

    //set up ignore and default
    ignore_action.sa_handler = SIG_IGN;
    default_action.sa_handler = SIG_DFL;

    //parent ignores SIGINT
    sigaction(SIGINT, &ignore_action, NULL);

    //parent SIGSTP handled by function
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    //main loop for each user command
    for(i=0; i<150; i++){

        //get command from user, stores raw command, args, input, output in global struct variable
        //return 1 if not comment or blank
        isExecutable = readCommand();

        //
        if(isExecutable){
            //do variable expansion
            expandVariables();
            //printCommandPieces();

            tokenizeArgs();
            //printCommandArgs();


            //check for built-in functions
            if(isBuiltInFunction()){
                //exit command
                if(strcmp(currArgs.command,"exit")==0){
                    //clean up child processes
                    killAllProcesses();
                    //restore original working directory
                    status_chdir = chdir(initCD);
                    if(status_chdir != 0){
                        printf("error resetting cd\n");
                    }
                    //exit program loop
                    break;
                }
                // cd command
                else if(strcmp(currArgs.command,"cd")==0){
                    updateCD();
                }
                // status command
                else if(strcmp(currArgs.command,"status")==0){
                    if(lastExit.isExit){
                        printf("exit value %d\n",lastExit.exitStatus);
                    }
                    else if(lastExit.isSignal){
                        printf("terminated by signal %d\n",lastExit.exitSignal);
                    }
                }
                else{
                    printf("Error interpreting built-in function\n");
                }
            }
            else{
            //if not built-in command, fork
                spawnPID = fork();

                if(spawnPID == -1){
                    perror("Error creating process\n");
                    //printf("%s\n", explain_fork());
                    printf("errno: %s\n", strerror(errno));
                    exit(1);
                }
                else if(spawnPID == 0){
                    //child process

                    //child processes handle signals differently
                    //child defuault action for SIGINT
                    sigaction(SIGINT, &default_action, NULL);
                    //child ignores SIGSTP
                    sigaction(SIGTSTP, &ignore_action, NULL);


                    runChildProcess();                

                    exit(1);
                }
                else{
                    //parent process
                    //if background store child pid, if not background wait for child pid

                    if(currCommand.isBackground == 0 || backgroundAvailable == 0){
                        //foreground process, wait until child finishes
                        waitpid(spawnPID, &childExitMethod, 0);

                        //exit status saved in global variable
                        saveExitStatus(childExitMethod);

                        //foreground child process terminated by signal -- then message to user
                        if(lastExit.isSignal){
                            printf("terminated by signal %d\n", lastExit.exitSignal);
                        }

                    }
                    else{
                        //background process AND background processing is available
                        //store child pid to check  later
                        backgroundPID.processes[backgroundPID.count] = spawnPID;
                        backgroundPID.count++;
                        printf("background pid is %d\n", spawnPID);

                    }

                }

            }
        }
        else{
            //printf("comment or empty line\n");
        }
        


        //check on background processes
        checkBackground();

    }   

    return 0;
}