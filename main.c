#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "pwd.h"
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
char* inputBuffer = NULL;
int waiting = 1;
int running = 1;
size_t buffSize;
command* cmd;

// Input Formatting Prototypes
int createCommandInstances(char* retBuffer);
char* filter(char* inputBuffer);
void getCommand();
int knownCommands();
void fillBlank(char* arr[], int size);
int getSize(char* argv[]);
char** formatArgs(char* args[], int max, int commandNum);
void signalHandler(int num);

// Basic Execution Prototypes
void performPipe(int leftCommandIndex);
void outputRedirect(int leftCommandIndex);
void outputAppendRedirect(int leftCommandIndex);
void inputRedirect(int leftCommandIndex);
void executeCommands(int commandSize);
void executeRC();
int setEnv(int commandIndex);
int unsetEnv();

// Sequential Execution Prototypes
void seqPipe(char* args[]);
void seqOutputRedirect(char* file);
void seqInputRedirect(char* file);
void executeSequentially(char* args[]);
char* sanitize(char *input);

// Debugging and Testing
void viewCommands(int size);

// ----- MAIN -----
int main(int argc, char* argv[])
{
    // Start by executing commands from RC file
    executeRC();

    FILE* env = fopen("pish.ev","wrb+");

    if (env == NULL)
    {
        printf("Created pish.ev\n");
    }

    fclose(env);

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
        int found = knownCommands();

        if (found)
            return;

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

    // More than 2 commands is handled by sequencing
    char* args[1024];

    char* sanitizedInput;
    sanitizedInput = sanitize(inputBuffer);
    
    
    if(sanitizedInput[strlen(sanitizedInput) - 1] == '&')
    {
        waiting = 0;
        sanitizedInput[strlen(sanitizedInput) - 1] = '\0';
    }
    

    char* arg = strtok(sanitizedInput, " ");
    int i = 0;
    while (arg)
    {
        if (strcmp(arg, "<") == 0)
        {
            seqInputRedirect(strtok(NULL, " "));
        }
        else if (strcmp(arg, ">") == 0)
        {
            seqOutputRedirect(strtok(NULL, " "));
        }
        else if (strcmp(arg, "|") == 0)
        {
            args[i] = NULL;
            seqPipe(args);
            i = 0;
        }
        else
        {
            args[i] = arg;
            i++;
        }
        arg = strtok(NULL, " ");
    }
    args[i] = NULL;

    executeSequentially(args);
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

    // Store Current Directory
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        printf("%s\n", cwd);

    // CD into user home
    uid_t uid = getuid();
    struct passwd* user = getpwuid(uid);
    if (!user)
    {
        printf("No user found\n");
        return;
    }
    chdir(user->pw_dir);

    // Open the file
    rc = fopen("pishrc", "r");

    // Check if pish.rc was opened
    if (rc == NULL)
    {
        printf("pishrc was not found\n");
        chdir(cwd);
        return;
    }

    // Execute pish.rc line by line
    while (fgets(buffer, buffSize, rc))
    {
        inputBuffer = malloc(buffSize + 1024);
        strcpy(inputBuffer, buffer);
        buffer[strlen(buffer)-1] = '\0';

        int commandSize = createCommandInstances(buffer);
        if (commandSize > 0)
        {
            printf("Executing: %s\n", inputBuffer);
            executeCommands(commandSize);
        }
    }

    // Close pish.rc
    fclose(rc);

    // Return To Original Directory
    chdir(cwd);
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

    // Copy retBuffer into inputBuffer

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

                if (fp)
                {
                    char envBuffer[100];
                    if(cmd[currCommandIndex].args[currArgIndex][0] == '$') {
                        char* newEnv = cmd[currCommandIndex].args[currArgIndex];
                        cmd[currCommandIndex].args[currArgIndex] = newEnv + 1;

                        while(fgets(envBuffer, 100, fp) != NULL) {
                        if(strstr(envBuffer, cmd[currCommandIndex].args[currArgIndex])) {
                            char* token1 = strtok(envBuffer, "=");
                            char* token = strtok(NULL, "\0");
                            cmd[currCommandIndex].args[currArgIndex] = token;
                        }
                    }
                    }  
                    fclose(fp);
                }
            
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

    // Move retBuffer to inputBuffer as backup
    inputBuffer = malloc(sizeof(retBuffer) + 1024);
    strcpy(inputBuffer, retBuffer);

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
    
    // No known command
    if (command == 0)
        return 0;
    // exit
    else if (command == 1)
    {
        printf("Exiting pish...\n");
        exit(-1);
    }
    // cd
    else if (command == 2)
    {
        if (cmd[0].args[0] == "NULL")
        {
            uid_t uid = getuid();
            struct passwd* user = getpwuid(uid);
            if (!user)
            {
                printf("No user found\n");
                return 1;
            }
            chdir(user->pw_dir);
        }
        else
            chdir(cmd[0].args[0]);
    }
    // help
    else if (command == 3)
    {
        printf("If it's not in the READEME, good luck\n");
    }
    // pwd
    else if (command == 4)
    {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf("%s\n", cwd);
    }
    else if (command == 5)
    {
        int res = setEnv(0);
    }
    else if (command == 6)
    {
        int res = unsetEnv();
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
    // Get size of the arguments
    int size = getSize(cmd[commandNum].args);

    // If no args, manually set the formatted args
    if (size == 0)
    {
        cmd[commandNum].cmd[strcspn(cmd[commandNum].cmd, "\n")] = 0;
        char** newArr = malloc(sizeof(char*) * 2);
        newArr[0] = cmd[commandNum].cmd;
        newArr[1] = NULL;
        return newArr;
    }

    // Remove trailing endline
    args[size - 1][strcspn(args[size - 1], "\n")] = 0;

    // Malloc Space for new array
    char** newArr = malloc(sizeof(char*) * (size + 2));
    int newSize = size + 2;

    // Populate new array with args
    for (int i = 1; i <= size; i++)
    {
        newArr[i] = args[i - 1];
        newArr[i][strcspn(newArr[i], "\n")] = 0;
    }

    // Set first and last element of the new array
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

    // Error Handling
    if (pipe(fd) == -1)
    {
        printf("Pipe no good\n");
        return;
    }

    // First Fork
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

    // Second Fork
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

    // Close File Decriptors
    close(fd[0]);
    close(fd[1]);

    // Wait for PIDs
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

    // Error Handling
    if(pid < 0)
    {
        printf("fork no good\n");
        exit(1);
    }

    // Child Process
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

    // Wait for Child Process
    waitpid(pid, NULL, 0);
}

//
// outputAppendRedirect: Performs an output append redirect (>>) on a command
// returns void
//
void outputAppendRedirect(int leftCommandIndex) {
    int pid = fork();

    // Error Handling
    if(pid < 0) {
        printf("fork no good\n");
        exit(1);
    }

    // Child Process
    if (pid == 0) {
        int fd = open(cmd[leftCommandIndex + 1].cmd,
            O_WRONLY | O_APPEND | O_CREAT,
            0666);
        dup2(fd, STDOUT_FILENO);
        close(fd);
        char** formattedArgs = formatArgs(cmd[leftCommandIndex].args, 10, leftCommandIndex);
        execvp(cmd[leftCommandIndex].cmd, formattedArgs);
    }

    // Wait for PID
    waitpid(pid, NULL, 0);
}

//
// inputRedirect: Performs an input redirect (<) on a command
// returns void
//
void inputRedirect(int leftCommandIndex) {
    int pid = fork();

    // Error Handling
    if (pid < 0) {
        printf("fork no good\n");
        exit(1);
    }

    // Child Process
    if (pid == 0) {
        int fd = open(cmd[leftCommandIndex + 1].cmd, O_RDONLY);
        dup2(fd, STDIN_FILENO);
        close(fd);
        char** formattedArgs = formatArgs(cmd[leftCommandIndex].args, 10, leftCommandIndex);
        execvp(cmd[leftCommandIndex].cmd, formattedArgs);
    }

    // Wait for PID
    waitpid(pid, NULL, 0);
}

//
// seqPipe: Performs a pipe when sequencing is required
// returns void
//
void seqPipe(char *args[])
{
    // Pipe File Descriptors
    int fd[2];
    pipe(fd);

    // Close Second File Descriptor
    dup2(fd[1], 1);
    close(fd[1]);

    // Run the Arguments Sequentially
    executeSequentially(args);

    // Close The First File Descriptor
    dup2(fd[0], 0);
    close(fd[0]);
}

//
// seqOutputRedirect: Performs an output redirect (>) when sequencing is required
// returns void
//
void seqOutputRedirect(char* file)
{
    // Open Output File
    int output = open
    (
        file,
        O_WRONLY | O_TRUNC | O_CREAT,
        0600
    );

    // Duplicate and Close Output
    dup2(output, 1);
    close(output);
}

//
// seqOutputRedirect: Performs an input redirect (<) when sequencing is required
// returns void
//
void seqInputRedirect(char* file)
{
    // Open Input File (READ ONLY)
    int input = open(file, O_RDONLY);

    // Duplicate and Close the File
    dup2(input, 0);
    close(input);
}

//
// executeSequentially: Handles execution when more than 2 commands exist
// returns void
//
void executeSequentially(char *args[])
{
    pid_t pid;
    if (strcmp(args[0], "exit\n") != 0)
        {
            pid = fork();
            if (pid < 0) { 
                fprintf(stderr, "Fork Failed");
            }
            else if ( pid == 0) { /* child process */
                execvp(args[0], args);
            }
            else { /* parent process */
                if (waiting) {
                    waitpid(pid, NULL, 0);
                } else {
                    waiting = 0;
                }
            }
            seqInputRedirect("/dev/tty");
            seqOutputRedirect("/dev/tty");
        }
    else {
        running = 0;
    }
}

//
// Sanitize: Handles execution when more than 2 commands exist
// returns a string of the sanitized input
//
char* sanitize(char *input)
{
    int i;
    int j = 0;

    // Create new sanitized string
    char *sanitizedInput = (char *)malloc((2048) * sizeof(char));

    // Loop through and format original input
    for (i = 0; i < strlen(input); i++) {
        if (input[i] != '>' && input[i] != '<' && input[i] != '|') {
            sanitizedInput[j++] = input[i];
        } else {
            sanitizedInput[j++] = ' ';
            sanitizedInput[j++] = input[i];
            sanitizedInput[j++] = ' ';
        }
    }
    sanitizedInput[j++] = '\0';

    char *end;
    end = sanitizedInput + strlen(sanitizedInput) - 1;
    end--;
    *(end + 1) = '\0';

    return sanitizedInput;
}

int setEnv(int commandIndex) {
    int MAXCHAR = 150;
    FILE *fp;
    char row[MAXCHAR];
    char *token;

    int res = unsetEnv();
    if(cmd[commandIndex].args[0] == "NULL") {
        fp = fopen("pish.ev","rb+");

        if (fp == NULL)
        {
            return 1;
        }

        fgets(row, MAXCHAR, fp);
        printf("%s", row);
        while (fgets(row, MAXCHAR, fp) != NULL) {
            printf("%s", row);
        }

        return 0;
    }
    else {
        fp = fopen("pish.ev","a+");

        if (fp == NULL)
        {
            return 1;
        }

        char * newLine = "\n";
        fwrite(cmd[commandIndex].args[0], strlen(cmd[commandIndex].args[0]), 1, fp);
        fwrite(newLine, strlen(newLine), 1, fp);
        fclose(fp);
        return 0;
    }
}

int unsetEnv() {
    FILE* src;
    FILE* temp;
    char buffer[100];
    src = fopen("pish.ev", "rb+");

    if (src == NULL)
    {
        return 1;
    }

    temp = fopen("temp.tmp", "w+");
    if(temp == NULL)
    {
        printf("Temp file could not be opened\n");
        return 1;
    }

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