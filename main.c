#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <signal.h>
#include<unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include<readline/readline.h>
#include<readline/history.h>

#define MAX 1<<12 // max buff
#define MAXCMDS 32 // max number of commands
#define MAXPIPES 32 // max number of pipes supported

int STDOUT; // custom STDOUT

// Function to print Current Directory.
void printDir(){
    char cwd[1024];
    char* username = getenv("USER");
    getcwd(cwd, sizeof(cwd));
    printf("%s@:%s$ ", username, cwd);
}

// Function to read and add to history
int readLine(char* str){
    printDir();
    char* buf = readline("");
    if (strlen(buf) != 0) {
        add_history(buf);
        strcpy(str, buf);
        return 0;
    } else {
        return 1;
    }
}

void CtrlC(int signal){
    printf("\n");
    printDir();
}

void CmdHandler(char *command){
    char *token, *args[MAXCMDS], nrOfArgs = 0;

    token = strtok(command," ");
    while(token!=NULL){
        args[nrOfArgs] = token;
        nrOfArgs++;
        token = strtok(NULL, " ");
    }

    args[nrOfArgs] = NULL;
    execvp(args[0], args);
}


void PipeHandler(char* inputString){

    char* commands[MAXPIPES], *token, nrOfCmds = 0, output[MAX];
    int offset, pipefd[2];


    // separating with the pipe symbol
    token = strtok(inputString, "|");
    while(token!=NULL){
        commands[nrOfCmds] = token;
        nrOfCmds++;
        token = strtok(NULL,"|");
    }
    commands[nrOfCmds] = NULL;

    for (int i = 0; i < nrOfCmds; ++i) { // two processes at each iteration
        pipe(pipefd);

        int pid = fork();

        if(pid < 0){
            fprintf(stderr, "Unable to fork.\n");
            exit(EXIT_FAILURE);
        }else if(pid == 0){ // child
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);

            lseek(STDOUT, 0, SEEK_SET);
            if(i != 0)
                dup2(STDOUT, STDIN_FILENO);

            CmdHandler(commands[i]); // This will write into STDOUT (not custom one)
            // child exits
        }else{ // parent
            close(pipefd[1]);
            offset = 0;
            while(read(pipefd[0], &output[offset], 1)!= 0) // read the output from the child
                offset++;
            close(pipefd[0]);

            lseek(STDOUT, 0, SEEK_SET);
            ftruncate(STDOUT, 0);
            write(STDOUT, output, offset);
        }
    }
}

void RedirectHandler(char* inputString){
    char* command,  *fileName;

    command = strtok(inputString,">");
    fileName = strtok(NULL, ">");

    if(fileName!=NULL) {
        int fd = open(fileName, O_RDWR | O_CREAT, S_IRWXU);
        dup2(fd, STDOUT);
        PipeHandler(command);
    } else {
        PipeHandler(command);
    }
}

int main(int argc, char* argv[]){
    char inputString[MAX], buff[MAX];
    int len;

    STDOUT = open("stdout", O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);

    signal(SIGINT, CtrlC );

    while (1) {
        // take input and add to history
        if (readLine(inputString)) // readLine returns 1 when nothing was typed
            continue;

        int pid = fork();

        if(pid == 0) {
            RedirectHandler(inputString);
            exit(EXIT_SUCCESS);
        }

        wait(NULL);
        lseek(STDOUT, 0, SEEK_SET);
        len = read(STDOUT, buff, MAX);

        write(STDOUT_FILENO, buff, len); // printing the output
    }
}
