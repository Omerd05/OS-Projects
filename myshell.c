#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>
#include <asm-generic/errno-base.h>

int listenSIGINT() {
    if(signal(SIGINT,SIG_DFL) == SIG_ERR) {
        fprintf(stderr,"An error regarding SIGINT occured.\n");
        return -1;
        //exit(1);
        //ERROR ERROR ERROR
    }
    return 1;
}

void ignoreSIGINT() {
    if(signal(SIGINT,SIG_IGN) == SIG_ERR) {
        fprintf(stderr,"An error regarding SIGINT occured.\n");
        exit(1);
        //ERROR ERROR ERROR
    }
}

//Slaying zombies, whoosh whoosh
void zombieSlayer() {
    //fprintf(stderr,"Slaying");
    int val;
    if((val = waitpid(-1,NULL,WNOHANG)) == ECHILD) {
        return;
    }
    while(val>0){
        //fprintf(stderr,"Slaying");
        if((val = waitpid(-1,NULL,WNOHANG)) == ECHILD) {
            return;
        }
    }
}

int prepare(void) {
    ignoreSIGINT();

    struct sigaction sa = {0};
    sa.sa_handler = zombieSlayer;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGCHLD,&sa,NULL)==-1) {
        fprintf(stderr,"An error modifying SIGCHLD occured.\n");
        return -1;
        //ERROR ERROR ERROR
    }

    return 0;
}

int elementFinder(char** ptr, char* target) {
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
    return result;
}

//If bgState = 0 we wait for child, otherwise bgState = 1 and we continue and ignore him.
int singleCommand(int count, char** arglist,int bgState) {
    pid_t cpid = fork();
    if(cpid == 0) { //Child
        if(bgState) { //i.e. if the last word is &
            arglist[count-1] = NULL; //Erasing & before execvp
        }
        else {
            if(listenSIGINT() == -1) {
                fprintf(stderr,"An error modifying handler for chils process occured.\n");
                return 0;
            }
        }
        if(execvp(arglist[0],arglist) == -1) {
            fprintf(stderr,"An error executing the command occured.\n");
            exit(0);
            //ERROR ERROR ERROR
        }
    }
    //Parent
    if((!bgState) && wait(NULL) == -1) {
        fprintf(stderr,"An error waiting for child process occured.\n");
        return -1;
        //ERROR ERROR ERROR
    }
    return 1;
}

int pipedCommand(int count, char** arglist, int pipeIdx) {
    // Child 1 | Child 2
    int pipefd[2];
    if(pipe(pipefd) == -1){
        fprintf(stderr,"An error piping occured.\n");
        exit(1);
    }
    pid_t cpid1 = fork();
    if(cpid1 == 0) { //i.e. child 1
        close(pipefd[0]);
        if(listenSIGINT() == -1) {
            fprintf(stderr,"An error modifying handler for chils process occured.\n");
            exit(0);
        }
        if(dup2(pipefd[1],1) == -1) {
            fprintf(stderr,"An error duping file descriptor occured.\n");
            exit(0);
        }
        arglist[pipeIdx] = NULL;
        if(execvp(arglist[0],arglist) == -1) {
            close(pipefd[1]);
            fprintf(stderr,"An error executing the command occured.\n");
            exit(0);
            //ERROR ERROR ERROR
        }
    }
    close(pipefd[1]);
    pid_t cpid2 = fork();
    if(cpid2 == 0) { //i.e. child 2
        if(listenSIGINT() == -1) {
            fprintf(stderr,"An error modifying handler for chils process occured.\n");
            return 0;
        }

        if(dup2(pipefd[0],0) == -1) {
            fprintf(stderr,"An error duping file descriptor occured.\n");
            return 0;
        }
        for(int i = 0; i+pipeIdx+1 <= count; i++) {
            arglist[i] = arglist[i+pipeIdx+1];
        }

        if(execvp(arglist[0],arglist) == -1) {
            close(pipefd[0]);
            fprintf(stderr,"An error executing the command occured.\n");
            //A day has been wasted on the following line :)
            //we have to kill the other child process or it's gonna be reading from the pipe and won't stop
            kill(cpid1,SIGTERM);
            exit(0);
            //ERROR ERROR ERROR
        }
    }
    //i.e. parent
    close(pipefd[0]);
    wait(NULL);
    wait(NULL);
    return 1;
}

int redirectCommandInput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_RDONLY | O_APPEND);
    pid_t cpid = fork();

    if(cpid == 0) { //i.e. child
        if(listenSIGINT() == -1) {
            fprintf(stderr,"An error modifying handler for chils process occured.\n");
            exit(0);
        }

        if(dup2(fd,0)==-1) {
            fprintf(stderr,"An error duping file descriptor occured.\n");
            exit(0);
            //ERROR ERROR ERROR
        }
        arglist[reDirIdx] = NULL;
        if(execvp(arglist[0],arglist)==-1) {
            fprintf(stderr,"An error executing the command occured.\n");
            exit(0);
            //ERROR ERROR ERROR
        }
    }
    //i.e. parent
    if(wait(NULL) == -1) {
        fprintf(stderr,"An error waiting for child process occured.\n");
        return -1;
        //ERROR ERROR ERROR
    }
    close(fd);
    return 1;
}

int redirectCommandOutput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_WRONLY | O_APPEND);
    pid_t cpid = fork();

    if(cpid == 0) { //i.e. child
        if(listenSIGINT() == -1) {
            fprintf(stderr,"An error modifying handler for chils process occured.\n");
            exit(0);
        }
        if(dup2(fd,1)==-1) {
            fprintf(stderr,"An error duping file descriptor occured.\n");
            exit(0);
            //ERROR ERROR ERROR
        }
        arglist[reDirIdx] = NULL;
        if(execvp(arglist[0],arglist)==-1) {
            fprintf(stderr,"An error executing the command occured.\n");
            exit(0);
            //ERROR ERROR ERROR
        }
    }
    //i.e. parent
    if(wait(NULL) == -1) {
        fprintf(stderr,"An error waiting for child process occured.\n");
        return -1;
        //ERROR ERROR ERROR
    }
    close(fd);
    return 0;
}

int process_arglist(int count, char** arglist) {
    int flagFast = strcmp(arglist[count-1],"&")==0; //flagFast is 1 if we dont wait, 0 if we do
    int flagPipe = 0;
    int flagRedirectInput = 0;
    int flagRedirectOutput = 0;

    int idxPipe = elementFinder(arglist,"|");
    int idxRedir1 = elementFinder(arglist,"<");
    int idxRedir2 = elementFinder(arglist,">>");


    if(idxPipe >= 0) {
        flagPipe = idxPipe;
    }
    if(idxRedir1 >= 0) {
        flagRedirectInput = idxRedir1;
    }
    if(idxRedir2 >= 0) {
        flagRedirectOutput = idxRedir2;
    }
    if(flagPipe) {
        //Error addressing requird here.
        pipedCommand(count,arglist,flagPipe);
        return 1;
    }
    if(flagFast) {
        //Error addressing requird here.
        if(singleCommand(count,arglist,flagFast)==-1) {
            return 0;
        }
        return 1;
    }
    if(flagRedirectInput) {
        //Error addressing requird here.
        if(redirectCommandInput(count,arglist,idxRedir1)==-1) {
            return 0;
        }
        return 1;
    }
    if(flagRedirectOutput) {
        //Error addressing requird here.
        if(redirectCommandOutput(count,arglist,idxRedir2)==-1) {
            return 0;
        }
        return 1;
    }
    if(singleCommand(count,arglist,flagFast)==-1) {
        return 0;
    }
    return 1;
}

int finalize(void) {
    return 0;
}
