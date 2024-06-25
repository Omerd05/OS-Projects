#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>

//Maybe a data structure to hold what elements needs to be freed?

int elementFinder(char** ptr, char* target) { //Maybe sz needs to be count-1
    char** curr = ptr;
    int timer = 0;
    int result = -1;
    while(*curr != NULL) {
        if(strcmp(*curr,target) == 0) {
            result = timer;
            break;
        }
        curr++;
        timer++;
    }
    //free(curr);
    return result;
}

int pipedCommand();
int singleCommand();
int redirectCommand();

int singleCommand(int count, char** arglist,int bgState) { //bgState = 1 - we wait, if 0 we continue
    pid_t cpid = fork();
    if(cpid == 0) { //Child
        if(execvp(arglist[0],arglist) == -1) {
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
    else { //Parent
        if(bgState) {
            if(wait(NULL) == -1) {
                //ERROR ERROR ERROR
                //Waiting error
            }
        }
    }
    return 0;
}

int pipedCommand(int count, char** arglist,int bgState, int pipeIdx) {
    // Child 1 | Child 2
    int pipefd[2];
    pipe(pipefd);
    pid_t cpid1 = fork();
    if(cpid1 != 0) { //i.e. parent
        close(pipefd[1]);
        pid_t cpid2 = fork();
        if(cpid2 != 0) { //i.e. parent
            close(pipefd[0]);
            if(wait(NULL)) {
                //ERROR ERROR ERROR
            }
            if(wait(NULL)) {
                //ERROR ERROR ERROR
            }
        }
        else { //i.e. child 2
            //Feels good about this finding -
            //Apparently, stdin, stdout and stderr are all files with fds 0,1,2 (in this order)
            //To our use, we can change what file descriptor with index 0 is pointing to
            dup2(pipefd[0],0);
            for(int i = 0; i+pipeIdx+1 <= count; i++) {
                arglist[i] = arglist[i+pipeIdx+1];
            }

            if(execvp(arglist[0],arglist) == -1) {
                //ERROR ERROR ERROR
                //Couldn't run the program
            }
        }
    }
    else { //i.e. child 1
        close(pipefd[0]);
        dup2(pipefd[1],1);
        arglist[pipeIdx] = NULL;
        if(execvp(arglist[0],arglist) == -1) {
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
    return 0;
}

int redirectCommand() {

    return 0;
}

int process_arglist(int count, char** arglist) {
    int flagPipe = 0;
    int idx = elementFinder(arglist,"|");
    if(idx >= 0) {
        flagPipe = idx;
    }
    int flagWait = !(strcmp(arglist[count-1],"&")==0);

    if(flagPipe) {
        pipedCommand(count,arglist,1,flagPipe);
    }
    else {
        char** passed;
        int cntCmd = count;
        if(!flagWait) {
            passed = malloc((count-1)*sizeof(char*));
            for(int i = 0; i < count-1; i++) {
                passed[i] = arglist[i];
            }
            passed[count-1] = arglist[count];
            cntCmd--;
        }
        else {
            passed = arglist;
        }

        singleCommand(cntCmd,passed,flagWait);
        if(!flagWait) {
            free(passed);
        }
    }

    return 1;
}
int prepare(void) {
    return 0;
}
int finalize(void) {
    return 0;
}
