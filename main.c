#include "stdio.h"
#include "stdlib.h"
#include <string.h>

char* retBuffer = NULL;
size_t buffSize;

typedef struct format
{
    long int numItems;
    char* argv[];
} format;

void parsing(char* retBuffer, format* junk);

int main(int argc, char* argv)
{
    getline(&retBuffer, &buffSize, stdin);
    format* junk = malloc(sizeof(format));
    parsing(retBuffer, junk);
    printf("%s\n", junk->argv[0]);
    return 0;
}

void parsing(char* retBuffer, format* junk)
{
    char* token;
    junk->numItems = (long)malloc(sizeof(int));
    junk->numItems = 0;
    while((token = strsep(&retBuffer, " ")) != NULL)
    {
        junk->argv[junk->numItems++] = token;
    }
}