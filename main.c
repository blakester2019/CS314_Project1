#include "stdio.h"
#include "stdlib.h"
#include <string.h>

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

        // Check for exit
        if (!strcmp(cmd->args[0], "exit"))
        {
            exit(-1);
        }
        
    }
    return 0;
}

//
// parse: parses input and populates a "command" type
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
//
void getCommand()
{
    printf("pish\%> ");
    getline(&retBuffer, &buffSize, stdin);
}