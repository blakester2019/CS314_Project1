#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "limits.h"
#include "sys/wait.h"

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
void parseCommands(int size);
int createCommandInstances(char* retBuffer);
void getCommand();
int knownCommands(char** args);
void fillBlank(char* arr[], int size);
void viewCommands(int size);
void performPipe(int leftCommandIndex);
int getSize(char* argv[]);
char** formatArgs(char* args[], int max, int commandNum);

// ----- MAIN -----
int main(int argc, char* argv[])
{
    while (1)
    {
        // Get User Input
        getCommand();

        // Get Size of Commands
        int size = createCommandInstances(retBuffer);

        // List Commands
        //viewCommands(size);

        
        int size2 = getSize(cmd[0].args);
        printf("Size2: %d\n", size2);
        for (int i = 0; i < size2; i++)
        {
            printf("args2[%d]: %s\n", i, cmd[0].args[i]);
        }
        
        performPipe(0);
    }
    return 0;
}

void parseCommands(int size)
{
    
}

//
// parse: parses input and populates a "command" type
// returns void
//
int createCommandInstances(char* retBuffer)
{
    char* backupBuffer[30];
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
            strcmp(token, ">") == 0
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
            strcmp(backupBuffer[j], ">") == 0
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
                currArgIndex++;
            }
        }
        j++;
    }
    
    return instancesToCreate;
}

//
// getCommand: takes user input
// returns void
//
void getCommand()
{
    printf("pish\%> ");
    getline(&retBuffer, &buffSize, stdin);
}

//
// Checks if first argument is a Pish command
// Returns 1 if a known command was executed and 0 if there was no known cmmand
//
int knownCommands(char** args)
{
    char* knownCommands[] = {"exit", "cd", "help", "pwd"};
    int size = 4;
    int command = 0;
    for (int i = 0; i < size; i++)
    {
        if (!strcmp(args[0], knownCommands[i]))
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
    
    return 1;
}

void fillBlank(char* arr[], int size)
{
    for (int i = 0; i < size; i++)
    {
        arr[i] = "NULL";
    }
}

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

int getSize(char* argv[])
{
    int i = -1;
    while (strcmp(argv[i++], "NULL") != 0 && argv[i] != NULL) {}
    return i - 1;
}

// FIX ME
char** formatArgs(char* args[], int max, int commandNum)
{
    printf("INTO FORMAT\n");
    int size = getSize(cmd[commandNum].args);
    printf("Size in format: %d\n", size);
    args[size - 1][strcspn(args[size - 1], "\n")] = 0;
    printf("This doesnt work\n");
    char** newArr = malloc(sizeof(char*) * (size + 2));
    int newSize = size + 1;

    for (int i = 1; i < size; i++)
    {
        newArr[i] = args[i - 1];
    }

    newArr[0] = cmd[commandNum].cmd;
    newArr[newSize] = NULL;

    printf("ABOUT TO RETURN FROM FORMAT\n");    

    return newArr;
}

void performPipe(int leftCommandIndex)
{
    printf("Performing pipe on: %d\n", leftCommandIndex);
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
        printf("Fork no good\n");
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