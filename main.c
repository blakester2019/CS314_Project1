#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "limits.h"

// Global Variables
char* retBuffer = NULL;
size_t buffSize;

// Data Structures
typedef struct command
{
    long int numItems;
    char** args;
} command;

// Prototypes
void parse(char* retBuffer, command* cmd);
void getCommand();
int knownCommands(char** args);

// ----- MAIN -----
int main(int argc, char* argv[])
{
    while (1)
    {
        // Get User Input
        getCommand();
        command* cmd = malloc(sizeof(command));

        // Parse Input
        parse(retBuffer, cmd);

        // Check for known commands
        knownCommands(cmd->args);
    }
    return 0;
}

//
// parse: parses input and populates a "command" type
// returns void
//
void parse(char* retBuffer, command* cmd)
{
    char* token;
    cmd->numItems = (long)malloc(sizeof(int));
    cmd->args = malloc(sizeof(char*) * cmd->numItems + 1);
    cmd->numItems = 0;
    while((token = strsep(&retBuffer, " ")) != NULL)
    {
        cmd->args[cmd->numItems++] = token;
    }
    // Remove endline character from last argument
    cmd->args[cmd->numItems - 1][strcspn(cmd->args[cmd->numItems - 1], "\n")] = 0;
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
        if 
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