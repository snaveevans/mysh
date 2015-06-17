#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/wait.h>

#define MAX_LINE        512 /* 512 chars per line, per command, should be enough. */
#define MAX_COMMANDS	5 /* size of history */

// Prototypes
void errorMessage();
int parser(char**, char[], char*);
char *trimWhitespace(char *);
void doPwd(char*);
void doLs(char*);
void doCd(char*);
void execute(char*, char*);
char* getInput();
typedef enum { false, true } bool;
int command_count = 0;

int main(void)
{
    char**jobs = malloc(MAX_LINE);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while(true)
    {
        char* input = getInput();

        int concurrentProcesses = parser(jobs, "&", input);

        int i;
        //loop through all jobs that must be done concurrently
        //forks will be made for those that can be executed concurrently
        //cd and exit are ones that can't be executed concurrently
        for(i = 0; i < concurrentProcesses; i++)
        {
            char *pch;
            pch=strchr(jobs[i],'>');
            bool hasRedirect = (pch!=NULL);

            //get commands for each job
            char**commands = malloc(MAX_LINE);
            //set file to null for every job
            char* file = NULL;
            int redirects = parser(commands, ">", jobs[i]);

            //there should never be more than one > (i used 2 because the int returned...
            //is the number of parameters that are passed back in)
            if(redirects > 2 ||(redirects == 1 && hasRedirect))
            {
                errorMessage();
                break;
            }
            //if there are 2 parameters that means there is 1 redirect set file name to second parameter
            else if(redirects == 2)
            {
                file = commands[1];
            }

            //send commands through
            execute(commands[0], file);

        }
        //wait for any child processes before continuing
        while(wait(NULL))
        {
            if(errno == ECHILD)
                break;
        }
    }
#pragma clang diagnostic pop

    return 0;
}

void execute(char* command, char* file)
{
    char** arguments = malloc(MAX_LINE);
    //find any additional arguments such as a new directory name
    int numberOfArguments = parser(arguments, " ", command);
    if(strcmp(command, "exit") == 0)
    {
        exit(1);
    }
    else if(strcmp(command, "wait") == 0)
    {
        //wait for any child processes before continuing
        while(wait(NULL))
        {
            if(errno == ECHILD)
                break;
        }
    }
    else if (strcmp(command, "pwd") == 0) {
        int rc = fork();
        if (rc == 0) {
            if (file == NULL) {
                doPwd(NULL);
            }
            else {
                doPwd(file);
            }
            exit(0); //exit so child process will stop and not continue execution
        }
    }
    else if(strcmp(command, "ls") == 0)
    {
        int rc = fork();
        int result;
        if (rc == 0)
        {
            if (file != NULL)
            {
                int outputRef = open(file, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
                close(1); //close the original stdout
                dup2(outputRef, 1); // make file new stdout
            }

            result = execvp(arguments[0], arguments);

            if (result == -1)
            {
                errorMessage();
            }
        }
    }
    else if(strcmp(command, "cd") == 0)
    {
        if(numberOfArguments == 1)
        {
            doCd(NULL);
        }
        else
        {
            doCd(arguments[1]);
        }
    }
    else
    {
        errorMessage();
    }
}

char* getInput()
{
    char *input = (char*)malloc(MAX_LINE + 1);
    int bytes_read = 0;

    //loop until something other than "\n" is read, which is 1 byte
    while(bytes_read <= 1)
    {
        printf("mysh>");
        fflush(stdout);
        bytes_read = read (STDIN_FILENO, input, MAX_LINE);
    }

    //remove newline character
    strtok(input, "\n");

    return input;
}

int parser(char** args, char token[], char* input)
{
    int size = 0;

    if(args == NULL)
    {
        printf("Error allocating memory!\n");
        return 1;
    }

    char* refined = malloc(sizeof(char*));
    refined = strtok(input, token);

    //split by token
    while(refined != NULL)
    {
        size++;
        args[size -1] = trimWhitespace(refined);
        refined = strtok(NULL, token);
    }


    return size;
}

char *trimWhitespace(char *str)
{
    char *end;

    // Trim leading space
    while(isspace(*str)) str++;

    if(*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace(*end)) end--;

    // Write new null terminator
    *(end+1) = 0;

    return str;
}

void doLs(char* file)
{
    DIR *dir;
    struct dirent *ent;
    char *directory = (char*)malloc(MAX_LINE + 1);
    getcwd(directory, MAX_LINE + 1);

    int outputRef;
    if(file != NULL)
    {
        outputRef = open(file, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
        if(outputRef < 0)
        {
            errorMessage();
            return;
        }
    }

    if ((dir = opendir (directory)) != NULL)
    {
        /* print all the files and directories within directory */
        while ((ent = readdir (dir)) != NULL)
        {
            if(file != NULL)
            {
                char* output = malloc(80);
                strcat(output, ent->d_name);
                strcat(output, "    ");
                write(outputRef, output, strlen(output));
            }
            else
            {
                printf ("%s    ", ent->d_name);
            }
        }
        closedir (dir);
    }
    else
    {
        /* could not open directory */
        errorMessage();
        return;
    }
    if(file != NULL)
    {
        (void) close(outputRef);
    }
    else
    {
        printf ("\n");
    }
}

void doCd(char* directory)
{
    const char* path = NULL;
    if(directory == NULL)
    {
        path = getenv("HOME");
    }
    else
    {
        path = directory;
    }
    int correct = chdir(path);
    if(correct != 0)
    {
        errorMessage();
    }
}

void doPwd(char* file)
{
    char *directory = (char*)malloc(MAX_LINE + 1);
    getcwd(directory, MAX_LINE + 1);
    if(file == NULL)
    {
        printf("%s\n", directory);
    }
    else
    {
        int outputRef = open(file, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
        if(outputRef < 0)
        {
            errorMessage();
            exit(1);
        }
        else
        {
            write(outputRef, directory, strlen(directory));
        }
        (void) close(outputRef);
    }
}

void errorMessage()
{
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}