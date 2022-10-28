#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "limits.h"
#include "sys/wait.h"
#include "fcntl.h"

typedef struct command
{
    char* cmd;
    char* args[10];
    char* separator;
} command;

// Global Variables
char* retBuffer = NULL;
size_t buffSize;
command* cmd;

// Prototypes
int createCommandInstances(char* retBuffer);
void getCommand();
int knownCommands();
void fillBlank(char* arr[], int size);
void viewCommands(int size);
void performPipe(int leftCommandIndex);
void outputRedirect(int leftCommandIndex);
void outputAppendRedirect(int leftCommandIndex);
void inputRedirect(int leftCommandIndex);
int getSize(char* argv[]);
char** formatArgs(char* args[], int max, int commandNum);
void executeCommands(int commandSize);
void executeRC();
int setEnv(int commandIndex);
int unsetEnv();
// void outputRedirect(int leftCommandIndex);
// void inputRedirect(int leftCommandIndex);

// ----- MAIN -----
int main(int argc, char* argv[])
{
    // Start by executing commands from RC file
    // executeRC();

    // Begin taking commands from user
    while (1)
    {
        // Get User Input
        getCommand();

        // Get How Many Commands To Run
        int commandSize = createCommandInstances(retBuffer);

        if (commandSize > 0)
        {
            executeCommands(commandSize);
        }
        
        // outputAppendRedirect(0);
    }
    return 0;
}

//
// executeCommands: executes the global array of commands
// returns void
//
void executeCommands(int commandSize)
{
    // If only 1 command, execute
    if (commandSize == 1)
    {
        // Check Known Commands
        knownCommands();

        int pid = fork();
        if (pid == 0)
        {
            char** formattedArgs = formatArgs(cmd[0].args, 10, 0);
            execvp(cmd[0].cmd, formattedArgs);
            printf("\n");
        }
        waitpid(pid, NULL, 0);
        return;
    }

    // 2 Commands, No Sequencing
    if (commandSize == 2)
    {
        if (strcmp(cmd[0].separator, "|") == 0)
            performPipe(0);
        else if (strcmp(cmd[0].separator, ">") == 0)
            outputRedirect(0);
        else if (strcmp(cmd[0].separator, ">>") == 0)
            outputAppendRedirect(0);
        else if (strcmp(cmd[0].separator, "<") == 0)
            inputRedirect(0);
        else
            printf("Invalid Separator\n");
        return;
    }

    //After this should handle sequencing
    char* args[45]; 
    int j = 0;
    for(int i = 0; i < commandSize; i++) {
        if(strcmp(cmd[i].separator, "<") == 0) {
            int inputDesc = open(cmd[i + 1].cmd, O_RDONLY);
            dup2(inputDesc, STDIN_FILENO);
            close(inputDesc);
        }
        else if (strcmp(cmd[i].separator, ">") == 0) {
            int outputDesc = open(cmd[i + 1].cmd, O_WRONLY | O_TRUNC | O_CREAT, 0666);
            dup2(outputDesc, STDOUT_FILENO);
            close(outputDesc);
        }
        args[j] = cmd[i].cmd;
        j++;
    }
    args[j] = NULL;
    int pid;
    pid = fork();
    if (pid < 0) { 
        fprintf(stderr, "Fork Failed");
    }
    else if ( pid == 0) { /* child process */
        execvp(args[0], args);
    }    
    waitpid(pid, NULL, 0);
}

//
// executeRC: executes the commands found in pish.rc
// returns void
//
void executeRC()
{
    FILE* rc;
    int buffSize = 255;
    char buffer[buffSize];

    // Open the file
    rc = fopen("pish.rc", "r");

    // Check if pish.rc was opened
    if (rc == NULL)
    {
        printf("pish.rc was not found\n");
        return;
    }

    // Execute pish.rc line by line
    while (fgets(buffer, buffSize, rc))
    {
        buffer[strlen(buffer)-1] = '\0';

        int commandSize = createCommandInstances(buffer);
        if (commandSize > 0)
        {
            printf("Executing: %s\n", buffer);
            executeCommands(commandSize);
        }
    }

    // Close pish.rc
    fclose(rc);
}

//
// createCommandInstances: populates the global array of commands
// returns number of commands to be run
//
int createCommandInstances(char* retBuffer)
{
    // If input is empty, create no instances
    if (strcmp(retBuffer, "") == 0)
        return 0;

    char* backupBuffer[30]; // retBuffer becomes null after reading tokens. This is a backup of retBuffer
    char* token;
    int instancesToCreate = 1;

    fillBlank(backupBuffer, 30);

    // Find the number of instances to create
    int i = 0;
    while((token = strsep(&retBuffer, " ")) != NULL)
    {
        if (
            strcmp(token, "|") == 0 ||
            strcmp(token, "<") == 0 ||
            strcmp(token, ">") == 0 ||
            strcmp(token, ">>") == 0 
        ) 
        {
            instancesToCreate++;
        }
        backupBuffer[i] = token;
        i++;
    }

    // Malloc Space for Instances
    cmd = malloc(sizeof(command) * instancesToCreate);
    for(i = 0; i < instancesToCreate; i++)
    {
        cmd[i].cmd = (char*)malloc(sizeof(char*));
        cmd[i].separator = (char*)malloc(sizeof(char*));
        cmd[i].separator = "NULL";
        fillBlank(cmd[i].args, 10);
    }

    // Creation Variables
    int initialCommand = 1;
    int currCommandIndex = 0;
    int currArgIndex = 0;
    int j = 0;

    // Begin creating command instances
    while (backupBuffer[j] != "NULL")
    {
        if (initialCommand == 1)
        {
            cmd[currCommandIndex].cmd = backupBuffer[j];
            currArgIndex = 0;
            initialCommand = 0;
        }
        else
        {
            if (
            strcmp(backupBuffer[j], "|") == 0 ||
            strcmp(backupBuffer[j], "<") == 0 ||
            strcmp(backupBuffer[j], ">") == 0 ||
            strcmp(backupBuffer[j], ">>") == 0 
            ) 
            {
                cmd[currCommandIndex].separator = backupBuffer[j];
                currCommandIndex++;
                currArgIndex = 0;
                initialCommand = 1;
            }
            else
            {
                cmd[currCommandIndex].args[currArgIndex] = backupBuffer[j];
                FILE* fp = fopen("pish.ev", "r");
                char bullShitbuffer[100];
                if(cmd[currCommandIndex].args[currArgIndex][0] == '$') {
                    char* newBS = cmd[currCommandIndex].args[currArgIndex];
                    cmd[currCommandIndex].args[currArgIndex] = newBS + 1;
                }   
                while(fgets(bullShitbuffer, 100, fp) != NULL) {
                    if(strstr(bullShitbuffer, cmd[currCommandIndex].args[currArgIndex])) {
                        char* token1 = strtok(bullShitbuffer, "=");
                        char* token = strtok(NULL, "\0");
                        cmd[currCommandIndex].args[currArgIndex] = token;
                    }
                }
                fclose(fp);
                currArgIndex++;
            }
        }
        j++;
    }
    
    // Return how many instances need to be created. This is useful during command execution
    return instancesToCreate;
}

//
// getCommand: takes user input
// returns void
//
void getCommand()
{
    // Take Input
    printf("pish\%> ");
    getline(&retBuffer, &buffSize, stdin);

    // Remove newline character at the end of the input
    retBuffer[strlen(retBuffer)-1] = '\0';
    fflush(stdin);
}

//
// Checks if first argument is a Pish command
// Returns 1 if a known command was executed and 0 if there was no known cmmand
//
int knownCommands()
{
    char* knownCommands[] = {"exit", "cd", "help", "pwd", "setenv", "unsetenv"};
    int size = 6;
    int command = 0;
    for (int i = 0; i < size; i++)
    {
        if (!strcmp(cmd[0].cmd, knownCommands[i]))
            command = i + 1;
    }
    
    // No knwon command
    if (command == 0)
        return 0;
    // exit
    else if (command == 1)
    {
        exit(-1);
    }
    // cd
    else if (command == 2)
    {
        printf("cd\n");
    }
    // help
    else if (command == 3)
    {
        printf("help\n");
    }
    // pwd
    else if (command == 4)
    {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf("%s\n", cwd);
    }
    else if (command == 5) {
        int lmao = setEnv(0);
    }
    else if (command == 6) {
        int lmao = unsetEnv();
    }
    
    return 1;
}

//
// fillBlank: fills an array if size {size} with string, "NULL"
// returns void
//
void fillBlank(char* arr[], int size)
{
    for (int i = 0; i < size; i++)
    {
        arr[i] = "NULL";
    }
}

//
// viewCommands: Debugging tool to view each element in the global command array
// returns void
//
void viewCommands(int size)
{
    for (int i = 0; i < size; i++)
    {
        printf("----- COMMAND %d -----\n", i);
        printf("cmd: %s\n", cmd[i].cmd);
        int j = 0;
        while (strcmp(cmd[i].args[j], "NULL") != 0)
        {
            printf("arg: %s\n", cmd[i].args[j]);
            j++;
        }
        printf("separator: %s\n", cmd[i].separator);
    }
}

//
// getSize: returns size of char* array
// returns size of the array
//
int getSize(char* argv[])
{
    int i = -1;
    while (strcmp(argv[i++], "NULL") != 0 && argv[i] != NULL) {}
    return i - 1;
}

//
// formatArgs: formats the arguments of a command to work with execvp
// returns array of the formatted arguments
//
char** formatArgs(char* args[], int max, int commandNum)
{
    int size = getSize(cmd[commandNum].args);

    if (size == 0)
    {
        cmd[commandNum].cmd[strcspn(cmd[commandNum].cmd, "\n")] = 0;
        char** newArr = malloc(sizeof(char*) * 2);
        newArr[0] = cmd[commandNum].cmd;
        newArr[1] = NULL;
        return newArr;
    }

    args[size - 1][strcspn(args[size - 1], "\n")] = 0;
    char** newArr = malloc(sizeof(char*) * (size + 2));
    int newSize = size + 2;

    for (int i = 1; i <= size; i++)
    {
        newArr[i] = args[i - 1];
        newArr[i][strcspn(newArr[i], "\n")] = 0;
    }

    newArr[0] = cmd[commandNum].cmd;
    newArr[newSize - 1] = NULL;

    return newArr;
}

//
// performPipe: Performs the pipe function
// returns void
//
void performPipe(int leftCommandIndex)
{
    int fd[2];

    if (pipe(fd) == -1)
    {
        printf("Pipe no good\n");
        return;
    }

    int pid1 = fork();
    if (pid1 < 0)
    {
        printf("Fork no good\n");
        return;
    }

    // Child Process
    if (pid1 == 0)
    {
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        char** formattedArgs = formatArgs(cmd[leftCommandIndex].args, 10, leftCommandIndex);
        execvp(cmd[leftCommandIndex].cmd, formattedArgs);
    }

    int pid2 = fork();
    if (pid2 < 0)
    {
        printf("Fork no gooOd\n");
        return;
    }

    // Child Process
    if (pid2 == 0)
    {
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        close(fd[1]);
        char** formattedArgs = formatArgs(cmd[leftCommandIndex + 1].args, 10, leftCommandIndex + 1);
        execvp(cmd[leftCommandIndex + 1].cmd, formattedArgs);
    }

    close(fd[0]);
    close(fd[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    return;
}

//
// outputRedirect: Performs an output redirect (>) on a command
// returns void
//
void outputRedirect(int leftCommandIndex) {
    int pid = fork();

    if(pid < 0)
    {
        printf("fork no good\n");
        exit(1);
    }

    if (pid == 0)
    {
        int fd = open(cmd[leftCommandIndex + 1].cmd,
            O_WRONLY | O_TRUNC | O_CREAT,
            0666);
        dup2(fd, STDOUT_FILENO);
        close(fd);
        char** formattedArgs = formatArgs(cmd[leftCommandIndex].args, 10, leftCommandIndex);
        execvp(cmd[leftCommandIndex].cmd, formattedArgs);
    }

    waitpid(pid, NULL, 0);
}

//
// outputAppendRedirect: Performs an output append redirect (>>) on a command
// returns void
//
void outputAppendRedirect(int leftCommandIndex) {
    int pid = fork();

    if(pid < 0) {
        printf("fork no good\n");
        exit(1);
    }

    if (pid == 0) {
        int fd = open(cmd[leftCommandIndex + 1].cmd,
            O_WRONLY | O_APPEND | O_CREAT,
            0666);
        dup2(fd, STDOUT_FILENO);
        close(fd);
        char** formattedArgs = formatArgs(cmd[leftCommandIndex].args, 10, leftCommandIndex);
        execvp(cmd[leftCommandIndex].cmd, formattedArgs);
    }

    waitpid(pid, NULL, 0);
}

//
// inputRedirect: Performs an input redirect (<) on a command
// returns void
//
void inputRedirect(int leftCommandIndex) {
    int pid = fork();

    if (pid < 0) {
        printf("fork no good\n");
        exit(1);
    }

    if (pid == 0) {
        int fd = open(cmd[leftCommandIndex + 1].cmd, O_RDONLY);
        dup2(fd, STDIN_FILENO);
        close(fd);
        char** formattedArgs = formatArgs(cmd[leftCommandIndex].args, 10, leftCommandIndex);
        execvp(cmd[leftCommandIndex].cmd, formattedArgs);
    }

    waitpid(pid, NULL, 0);
}

int setEnv(int commandIndex) {
    int MAXCHAR = 150;
    FILE *fp;
    char row[MAXCHAR];
    char *token;

    int lmao = unsetEnv();
    if(cmd[commandIndex].args[0] == "NULL") {
        fp = fopen("pish.ev","r");
        fgets(row, MAXCHAR, fp);
        printf("%s", row);
        while (fgets(row, MAXCHAR, fp) != NULL) {
            printf("%s", row);
        }
        return 0;
    }
    else {
        fp = fopen("pish.ev","a+");
        char * lol = "\n";
        fwrite(cmd[commandIndex].args[0], strlen(cmd[commandIndex].args[0]), 1, fp);
        fwrite(lol, strlen(lol), 1, fp);
        fclose(fp);
        return 0;
    }
}

int unsetEnv() {
    FILE* src;
    FILE* temp;
    char buffer[100];
    src = fopen("pish.ev", "r");
    temp = fopen("temp.tmp", "w+");
    if(temp == NULL) {
        printf("dawg the file aint open");
    }
    //fgets(buffer, 100, src);
    //printf("%s", buffer);
    // printf("%s", cmd[commandIndex].args[0]);
    while(fgets(buffer, 100, src) != NULL) {
        if((strstr(buffer, cmd[0].args[0])) == NULL) {
            fputs(buffer, temp);
        }
    }   
    fclose(src);
    fclose(temp);
    remove("pish.ev");
    rename("temp.tmp", "pish.ev");
    return 0;
}