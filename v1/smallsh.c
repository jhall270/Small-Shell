#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

//#include <libexplain/fork.h>
#include <errno.h>


void expandVariableInArray(char* arr);
void expandVariables();
void writePIDstring(char* pidString);
void itoa(int n, char s[]);
void reverse(char s[]);

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

//global variable to store currently inputted command from user
struct commandLine currCommand;
struct commandArgs currArgs;

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


//source note: this function is based off of userinput_adv.c sample code
void catchSIGINT(int signo)
{
  char* message = "SIGINT. Use CTRL-Z to Stop.\n";
  write(STDOUT_FILENO, message, 28);
}
//source note: this function is based off of userinput_adv.c sample code
//returns 1 if user line matches pattern of a command
//return 0 if it is blank line or commment (no command to execute)
int readCommand()
{
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = catchSIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    //SIGINT_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &SIGINT_action, NULL);

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

//this function cleans up the strings in the currCommand struct
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

void printCommandArgs(){
    int i;
    printf("command %s\n", currArgs.command);
    printf("numArgs %i\n", currArgs.numArgs);
    for(i=0; i<currArgs.numArgs; i++){
        printf("arg %i %s\n", i, currArgs.args[i]);
    }

}





int isBuiltInFunction(){
    return 0;
}

//runs non-built-in command in child process
//sets up input/output redirection
//makes execvp call
void runChildProcess(){
    int execStatus=0;

    if(currCommand.isInputRedir){
        //redirect input

    }

    if(currCommand.isOutputRedir){
        //redirect output
    }


    //set up NULL termination of arguments
    currArgs.args[currArgs.numArgs] = NULL;
    //try to execute command
    printf("Executing command: %s\n",currArgs.command);
    execStatus = execvp(currArgs.command, currArgs.args);

    if(execStatus == -1){
        printf("Error: Unable to execute command %s\n",currArgs.command);
    }

    return;
}



int main(){
    int isExecutable; // 0 if comment or empty, 1 if command
    pid_t spawnPID =-5;
    int childExitMethod = -5;
    int exitStatus, termSignal; //store info on exited child process

    //get command from user, stores raw command, args, input, output in global struct variable
    //return 1 if not comment or blank
    isExecutable = readCommand();

    //
    if(isExecutable){
        //do variable expansion
        expandVariables();
        printCommandPieces();

        tokenizeArgs();
        printCommandArgs();


        //check for built-in functions
        if(isBuiltInFunction()){

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
                printf("Child process -- begin child process set up\n");
                runChildProcess();                

                exit(1);
            }
            else{
                //parent process
                

                if(currCommand.isBackground == 0){
                    printf("Parent process -- waiting for child to finish \n");
                    //foreground process, wait until child finishes
                    waitpid(spawnPID, &childExitMethod, 0);

                    
                    if(WIFEXITED(childExitMethod)){
                        printf("Process exited normally\n");
                        exitStatus = WEXITSTATUS(childExitMethod);
                        printf("exit status was %d\n", exitStatus);
                    }

                    if(WIFSIGNALED(childExitMethod)){
                        printf("Process terminated by signal\n");
                        termSignal = WTERMSIG(childExitMethod);
                        printf("exit term status was %d\n", termSignal);
                    }

                }
                else{
                    //background process
                    //store child pid to check  later



                }

            }

        }
    }
    else{
        printf("comment or empty line\n");
    }
    


    //check on background processes

    

    return 0;
}