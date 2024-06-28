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





/*
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
            //return 0;
            //ERROR ERROR ERROR
        }
    }
    else { //Parent
        if((!bgState) && wait(NULL) == -1) {
            fprintf(stderr,"An error waiting for child process occured.\n");
            return -1;
            //ERROR ERROR ERROR
        }
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
            //return 0;
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
            //fprintf(stderr,"Proof of concept 2\n");
            kill(cpid1,SIGTERM);
            exit(0);
            //ERROR ERROR ERROR
        }
    }
    //i.e. parent
    close(pipefd[0]);
    wait(NULL);
    wait(NULL);
    /*if(wait(NULL) == -1) {
        fprintf(stderr,"An error waiting for child process occured.\n");
        return -1;
        //ERROR ERROR ERROR
    }
    if(wait(NULL) == -1) {
        fprintf(stderr,"An error waiting for child process occured.\n");
        return -1;
    }*/
    return 1;
}

int redirectCommandInput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_RDONLY | O_APPEND);
    pid_t cpid = fork();
    if(cpid != 0) { //i.e. Parent
        if(wait(NULL) == -1) {
            fprintf(stderr,"An error waiting for child process occured.\n");
            return -1;
            //ERROR ERROR ERROR
        }
        close(fd);
    }
    else { //i.e. Child
        if(listenSIGINT() == -1) {
            fprintf(stderr,"An error modifying handler for chils process occured.\n");
            exit(0);
            //return 0;
        }

        if(dup2(fd,0)==-1) {
            fprintf(stderr,"An error duping file descriptor occured.\n");
            exit(0);
            //return 0;
            //ERROR ERROR ERROR
        }
        arglist[reDirIdx] = NULL;
        if(execvp(arglist[0],arglist)==-1) {
            fprintf(stderr,"An error executing the command occured.\n");
            exit(0);
            //return 0;
            //ERROR ERROR ERROR
        }
    }
    return 1;
}

int redirectCommandOutput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_WRONLY | O_APPEND);
    pid_t cpid = fork();
    if(cpid != 0) { //i.e. Parent
        if(wait(NULL) == -1) {
            fprintf(stderr,"An error waiting for child process occured.\n");
            return -1;
            //ERROR ERROR ERROR
        }
        close(fd);
    }
    else { //i.e. Child
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
        if(pipedCommand(count,arglist,flagPipe)==-1) {
            return 0;
        }
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





/*
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>

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
    //int status;
    pid_t pid;

    while(1) {
        pid = wait3(NULL,WNOHANG,(struct rusage *)NULL);
        if(pid==0) {
            return;
        }
        if(pid==-1) {
            return;
        }
    }
    //int val = waitpid(-1,&status,WNOHANG);
    //while(val>0){
    //    val = waitpid(-1,&status,WNOHANG);
    //}
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
            //return 0;
            //ERROR ERROR ERROR
        }
    }
    else { //Parent
        if((!bgState) && wait(NULL) == -1) {
            fprintf(stderr,"An error waiting for child process occured.\n");
            return -1;
            //ERROR ERROR ERROR
        }
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
    if(cpid1 != 0) { //i.e. parent
        close(pipefd[1]);
        pid_t cpid2 = fork();
        if(cpid2 != 0) { //i.e. parent
            close(pipefd[0]);

            //MAYBE USE WAITPID INSTEAD HERE?

            if(wait(NULL) == -1) {
                fprintf(stderr,"An error waiting for child process occured.\n");
                return -1;
                //ERROR ERROR ERROR
            }
            if(wait(NULL) == -1) {
                fprintf(stderr,"An error waiting for child process occured.\n");
                return -1;
            }
        }
        else { //i.e. child 2
            //A cool finding -
            //Apparently, stdin, stdout and stderr are all fds numbered 0,1,2 (in this order)
            //We can use that to change what file descriptor with index 0 is pointing to
            if(listenSIGINT() == -1) {
                fprintf(stderr,"An error modifying handler for chils process occured.\n");
                return 0;
            }

            if(dup2(pipefd[0],0)) {
                fprintf(stderr,"An error duping file descriptor occured.\n");
                return 0;
            }
            for(int i = 0; i+pipeIdx+1 <= count; i++) {
                arglist[i] = arglist[i+pipeIdx+1];
            }

            if(execvp(arglist[0],arglist) == -1) {
                close(pipefd[0]);
                fprintf(stderr,"An error executing the command occured.\n");
                fprintf(stderr,"Proof of concept 2\n");

                exit(0);
                //ERROR ERROR ERROR
            }
        }
    }
    else { //i.e. child 1
        if(listenSIGINT() == -1) {
            fprintf(stderr,"An error modifying handler for chils process occured.\n");
            exit(0);
        }
        close(pipefd[0]);
        dup2(pipefd[1],1);
        arglist[pipeIdx] = NULL;
        if(execvp(arglist[0],arglist) == -1) {
            //fprintf(stderr,"Proof of concept 1\n");
            fprintf(stderr,"An error executing the command occured.\n");
            exit(0);
            //ERROR ERROR ERROR
        }
    }
    return 1;
}

int redirectCommandInput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_RDONLY | O_APPEND);
    pid_t cpid = fork();
    if(cpid != 0) { //i.e. Parent
        if(wait(NULL) == -1) {
            fprintf(stderr,"An error waiting for child process occured.\n");
            return -1;
            //ERROR ERROR ERROR
        }
        close(fd);
    }
    else { //i.e. Child
        if(listenSIGINT() == -1) {
            fprintf(stderr,"An error modifying handler for chils process occured.\n");
            exit(0);
            //return 0;
        }

        if(dup2(fd,0)==-1) {
            fprintf(stderr,"An error duping file descriptor occured.\n");
            exit(0);
            //return 0;
            //ERROR ERROR ERROR
        }
        arglist[reDirIdx] = NULL;
        if(execvp(arglist[0],arglist)==-1) {
            fprintf(stderr,"An error executing the command occured.\n");
            exit(0);
            //return 0;
            //ERROR ERROR ERROR
        }
    }
    return 1;
}

int redirectCommandOutput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_WRONLY | O_APPEND);
    pid_t cpid = fork();
    if(cpid != 0) { //i.e. Parent
        if(wait(NULL) == -1) {
            fprintf(stderr,"An error waiting for child process occured.\n");
            return -1;
            //ERROR ERROR ERROR
        }
        close(fd);
    }
    else { //i.e. Child
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
        if(pipedCommand(count,arglist,flagPipe)==-1) {
            return 0;
        }
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



/*#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>

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
    int status;
    //int timer = 0;
    int val = waitpid(-1,&status,WNOHANG);
    while(val>0) {
        val = waitpid(-1,&status,WNOHANG);
        //printf("in zombie slayer %d", timer++);
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
            //return 0;
            //ERROR ERROR ERROR
        }
    }
    else { //Parent
        if((!bgState) && wait(NULL) == -1) {
            fprintf(stderr,"An error waiting for child process occured.\n");
            return -1;
            //ERROR ERROR ERROR
        }
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
    if(cpid1 != 0) { //i.e. parent
        close(pipefd[1]);
        pid_t cpid2 = fork();
        if(cpid2 != 0) { //i.e. parent
            close(pipefd[0]);
            if(wait(NULL) == -1) {
                fprintf(stderr,"An error waiting for child process occured.\n");
                return -1;
                //ERROR ERROR ERROR
            }
            if(wait(NULL) == -1) {
                fprintf(stderr,"An error waiting for child process occured.\n");
                return -1;
            }
        }
        else { //i.e. child 2
            //A cool finding -
            //Apparently, stdin, stdout and stderr are all fds numbered 0,1,2 (in this order)
            //We can use that to change what file descriptor with index 0 is pointing to
            if(listenSIGINT() == -1) {
                fprintf(stderr,"An error modifying handler for chils process occured.\n");
                return 0;
            }

            if(dup2(pipefd[0],0)) {
                fprintf(stderr,"An error duping file descriptor occured.\n");
                return 0;
            }
            dup2(pipefd[0],0);
            for(int i = 0; i+pipeIdx+1 <= count; i++) {
                arglist[i] = arglist[i+pipeIdx+1];
            }

            if(execvp(arglist[0],arglist) == -1) {
                fprintf(stderr,"An error executing the command occured.\n");
                exit(0);
                return 0;
                //ERROR ERROR ERROR
            }
        }
    }
    else { //i.e. child 1
        if(listenSIGINT() == -1) {
            fprintf(stderr,"An error modifying handler for chils process occured.\n");
            exit(0);
        }
        close(pipefd[0]);
        dup2(pipefd[1],1);
        arglist[pipeIdx] = NULL;
        if(execvp(arglist[0],arglist) == -1) {
            //fprintf(stderr,"Proof of concept 1\n");
            fprintf(stderr,"An error executing the command occured.\n");
            exit(0);
            //ERROR ERROR ERROR
        }
    }
    return 1;
}

int redirectCommandInput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_RDONLY | O_APPEND);
    pid_t cpid = fork();
    if(cpid != 0) { //i.e. Parent
        if(wait(NULL) == -1) {
            fprintf(stderr,"An error waiting for child process occured.\n");
            return -1;
            //ERROR ERROR ERROR
        }
        close(fd);
    }
    else { //i.e. Child
        if(listenSIGINT() == -1) {
            fprintf(stderr,"An error modifying handler for chils process occured.\n");
            return 0;
        }

        if(dup2(fd,0)==-1) {
            fprintf(stderr,"An error duping file descriptor occured.\n");
            return 0;
            //ERROR ERROR ERROR
        }
        arglist[reDirIdx] = NULL;
        if(execvp(arglist[0],arglist)==-1) {
            fprintf(stderr,"An error executing the command occured.\n");
            exit(0);
            //return 0;
            //ERROR ERROR ERROR
        }
    }
    return 1;
}

int redirectCommandOutput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_WRONLY | O_APPEND);
    pid_t cpid = fork();
    if(cpid != 0) { //i.e. Parent
        if(wait(NULL) == -1) {
            fprintf(stderr,"An error waiting for child process occured.\n");
            return -1;
            //ERROR ERROR ERROR
        }
        close(fd);
    }
    else { //i.e. Child
        if(listenSIGINT() == -1) {
            fprintf(stderr,"An error modifying handler for chils process occured.\n");
            return 0;
        }
        if(dup2(fd,1)==-1) {
            fprintf(stderr,"An error duping file descriptor occured.\n");
            return 0;
            //ERROR ERROR ERROR
        }
        arglist[reDirIdx] = NULL;
        if(execvp(arglist[0],arglist)==-1) {
            fprintf(stderr,"An error executing the command occured.\n");
            exit(0);
            //return 0;
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
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
        if(pipedCommand(count,arglist,flagPipe)==-1) {
            return 0;
        }
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


/*
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>
#include <asm-generic/errno-base.h>

//void handler(int signum) {printf("Can't stop me! (%d)\n", signum);}

void listenSIGINT() {
    if(signal(SIGINT,SIG_DFL) == SIG_ERR) {
        fprintf(stderr,"An error regarding SIGINT occured.\n");
        exit(1);
        //ERROR ERROR ERROR
    }
}

void ignoreSIGINT() {
    if(signal(SIGINT,SIG_IGN) == SIG_ERR) {
        fprintf(stderr,"An error regarding SIGINT occured.\n");
        exit(1);
        //ERROR ERROR ERROR
    }

    /*struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; //I've spent 2 hours trying to fix the handling, apparently this line fix it
    if(sigaction(SIGINT, &sa, NULL) == -1) {
        //ERROR ERROR ERROR
    }*/
}

//Slaying zombies, whoosh whoosh
void zombieSlayer() {
    int status;
    int val = waitpid(-1,&status,WNOHANG);
    while(val>0) {
        val = waitpid(-1,&status,WNOHANG);
    }
    if(val == -1) {
        fprintf(stderr,"An error killing zombies occured.\n");
        exit(1);
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
    return result;
}

int singleCommand(int count, char** arglist,int bgState) { //bgState = 0 - we wait, if 1 we continue
    pid_t cpid = fork();
    if(cpid == 0) { //Child
        if(bgState) { //i.e. if the last word is &
            arglist[count-1] = NULL; //Erasing & before execvp
        }
        else {
            listenSIGINT();
        }
        if(execvp(arglist[0],arglist) == -1) {
            fprintf(stderr,"An error executing the command occured.\n");
            return 1;
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
    else { //Parent
        if((!bgState) && wait(NULL) == -1) {
            //if(wait(NULL) == -1) {
                //ERROR ERROR ERROR
                //Waiting error
            //}
        }
    }
    return 0;
}

int pipedCommand(int count, char** arglist, int pipeIdx) {
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
            listenSIGINT();
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
        listenSIGINT();
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

int redirectCommandInput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_RDONLY | O_APPEND);
    pid_t cpid = fork();
    if(cpid != 0) { //i.e. Parent
        if(wait(NULL)) {
            return -1;
            //ERROR ERROR ERROR
        }
        close(fd);
    }
    else { //i.e. Child
        listenSIGINT();
        if(dup2(fd,0)==-1) {
            return -1;
            //ERROR ERROR ERROR
        }
        arglist[reDirIdx] = NULL;
        if(execvp(arglist[0],arglist)==-1) {
            return -1;
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
    return 0;
}

int redirectCommandOutput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_WRONLY | O_APPEND);
    pid_t cpid = fork();
    if(cpid != 0) { //i.e. Parent
        if(wait(NULL)) {
            return -1;
            //ERROR ERROR ERROR
        }
        close(fd);
    }
    else { //i.e. Child
        listenSIGINT();
        if(dup2(fd,1)==-1) {
            return -1;
            //ERROR ERROR ERROR
        }
        arglist[reDirIdx] = NULL;
        if(execvp(arglist[0],arglist)==-1) {
            return -1;
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
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
    }
    if(flagFast) {
        //Error addressing requird here.
        singleCommand(count,arglist,flagFast);
        return 1;
    }
    if(flagRedirectInput) {
        //Error addressing requird here.
        redirectCommandInput(count,arglist,idxRedir1);
        return 1;
    }
    if(flagRedirectOutput) {
        //Error addressing requird here.
        redirectCommandOutput(count,arglist,idxRedir2);
        return 1;
    }
    singleCommand(count,arglist,flagFast);
    return 1;
}

int finalize(void) {
    return 0;
}


/*#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>

//Maybe a data structure to kill zombies?

//void handler(int signum) {printf("Can't stop me! (%d)\n", signum);}

void listenSIGINT() {
    if(signal(SIGINT,SIG_DFL) == SIG_ERR) {
        //ERROR ERROR ERROR
    }
}

void ignoreSIGINT() {
    if(signal(SIGINT,SIG_IGN) == SIG_ERR) {
        //ERROR ERROR ERROR
    }

    /*struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; //I've spent 2 hours trying to fix the handling, apparently this line fix it
    if(sigaction(SIGINT, &sa, NULL) == -1) {
        //ERROR ERROR ERROR
    }*/
}

int prepare(void) {
    ignoreSIGINT();
    return 0;
}

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

int singleCommand(int count, char** arglist,int bgState) { //bgState = 0 - we wait, if 1 we continue
    pid_t cpid = fork();
    if(cpid == 0) { //Child
        if(bgState) { //i.e. if the last word is &
            arglist[count-1] = NULL; //Erasing & before execvp
        }
        else {
            listenSIGINT();
        }
        if(execvp(arglist[0],arglist) == -1) {
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
    else { //Parent
        if((!bgState) && wait(NULL) == -1) {
            //if(wait(NULL) == -1) {
                //ERROR ERROR ERROR
                //Waiting error
            //}
        }
    }
    return 0;
}

int pipedCommand(int count, char** arglist, int pipeIdx) {
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
            listenSIGINT();
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
        listenSIGINT();
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

int redirectCommandInput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_RDONLY | O_APPEND);
    pid_t cpid = fork();
    if(cpid != 0) { //i.e. Parent
        if(wait(NULL)) {
            return -1;
            //ERROR ERROR ERROR
        }
        close(fd);
    }
    else { //i.e. Child
        listenSIGINT();
        if(dup2(fd,0)==-1) {
            return -1;
            //ERROR ERROR ERROR
        }
        arglist[reDirIdx] = NULL;
        if(execvp(arglist[0],arglist)==-1) {
            return -1;
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
    return 0;
}

int redirectCommandOutput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_WRONLY | O_APPEND);
    pid_t cpid = fork();
    if(cpid != 0) { //i.e. Parent
        if(wait(NULL)) {
            return -1;
            //ERROR ERROR ERROR
        }
        close(fd);
    }
    else { //i.e. Child
        listenSIGINT();
        if(dup2(fd,1)==-1) {
            return -1;
            //ERROR ERROR ERROR
        }
        arglist[reDirIdx] = NULL;
        if(execvp(arglist[0],arglist)==-1) {
            return -1;
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
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
    }
    if(flagFast) {
        //Error addressing requird here.
        singleCommand(count,arglist,flagFast);
        return 1;
    }
    if(flagRedirectInput) {
        //Error addressing requird here.
        redirectCommandInput(count,arglist,idxRedir1);
        return 1;
    }
    if(flagRedirectOutput) {
        //Error addressing requird here.
        redirectCommandOutput(count,arglist,idxRedir2);
        return 1;
    }
    singleCommand(count,arglist,flagFast);
    return 1;
}

int finalize(void) {
    return 0;
}



/*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>

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

int singleCommand(int count, char** arglist,int bgState) { //bgState = 0 - we wait, if 1 we continue
    pid_t cpid = fork();
    if(cpid == 0) { //Child
        if(execvp(arglist[0],arglist) == -1) {
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
    else { //Parent
        if(!bgState) {
            if(wait(NULL) == -1) {
                //ERROR ERROR ERROR
                //Waiting error
            }
        }
    }
    return 0;
}

int pipedCommand(int count, char** arglist, int pipeIdx) {
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

int redirectCommandInput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_RDONLY | O_APPEND);
    pid_t cpid = fork();
    if(cpid != 0) { //i.e. Parent
        if(wait(NULL)) {
            return -1;
            //ERROR ERROR ERROR
        }
        close(fd);
    }
    else { //i.e. Child
        if(dup2(fd,0)==-1) {
            return -1;
            //ERROR ERROR ERROR
        }
        arglist[reDirIdx] = NULL;
        if(execvp(arglist[0],arglist)==-1) {
            return -1;
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
    return 0;
}

int redirectCommandOutput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_WRONLY | O_APPEND);
    pid_t cpid = fork();
    if(cpid != 0) { //i.e. Parent
        if(wait(NULL)) {
            return -1;
            //ERROR ERROR ERROR
        }
        close(fd);
    }
    else { //i.e. Child
        if(dup2(fd,1)==-1) {
            return -1;
            //ERROR ERROR ERROR
        }
        arglist[reDirIdx] = NULL;
        if(execvp(arglist[0],arglist)==-1) {
            return -1;
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
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
        pipedCommand(count,arglist,flagPipe);
        return 1;
    }
    if(flagFast) {
        char** passed;
        int cntCmd = count;
        passed = malloc((count-1)*sizeof(char*));
        for(int i = 0; i < count-1; i++) {
            passed[i] = arglist[i];
        }
        passed[count-1] = arglist[count];
        cntCmd--;
        singleCommand(cntCmd,passed,flagFast);
        free(passed);
        return 1;
    }
    if(flagRedirectInput) {
        redirectCommandInput(count,arglist,idxRedir1);
        return 1;
    }
    if(flagRedirectOutput) {
        redirectCommandOutput(count,arglist,idxRedir2);
        return 1;
    }
    singleCommand(count,arglist,flagFast);
    return 1;
}
int prepare(void) {
    return 0;
}
int finalize(void) {
    return 0;
}

/*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>

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

int singleCommand(int count, char** arglist,int bgState) { //bgState = 0 - we wait, if 1 we continue
    pid_t cpid = fork();
    if(cpid == 0) { //Child
        if(execvp(arglist[0],arglist) == -1) {
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
    else { //Parent
        if(!bgState) {
            if(wait(NULL) == -1) {
                //ERROR ERROR ERROR
                //Waiting error
            }
        }
    }
    return 0;
}

int pipedCommand(int count, char** arglist, int pipeIdx) {
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

int redirectCommandInput(int count, char** arglist, int reDirIdx) {
    int fd = open(arglist[count-1],O_RDONLY);
    pid_t cpid = fork();
    if(cpid != 0) { //i.e. Parent
        if(wait(NULL)) {
            return -1;
            //ERROR ERROR ERROR
        }
        close(fd);
    }
    else { //i.e. Child
        if(dup2(fd,0)==-1) {
            return -1;
            //ERROR ERROR ERROR
        }
        arglist[reDirIdx] = NULL;
        if(execvp(arglist[0],arglist)==-1) {
            return -1;
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
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
        pipedCommand(count,arglist,flagPipe);
        return 1;
    }
    if(flagFast) {
        char** passed;
        int cntCmd = count;
        passed = malloc((count-1)*sizeof(char*));
        for(int i = 0; i < count-1; i++) {
            passed[i] = arglist[i];
        }
        passed[count-1] = arglist[count];
        cntCmd--;
        singleCommand(cntCmd,passed,flagFast);
        free(passed);
        return 1;
    }
    if(flagRedirectInput) {
        redirectCommandInput(count,arglist,idxRedir1);
        return 1;
    }
    if(flagRedirectOutput) {

        return 1;
    }

    singleCommand(count,arglist,flagFast);
    return 1;
}
int prepare(void) {
    return 0;
}
int finalize(void) {
    return 0;
}
*/
/*#include <stdio.h>
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

int singleCommand(int count, char** arglist,int bgState) { //bgState = 0 - we wait, if 1 we continue
    pid_t cpid = fork();
    if(cpid == 0) { //Child
        if(execvp(arglist[0],arglist) == -1) {
            //ERROR ERROR ERROR
            //Couldn't run the program
        }
    }
    else { //Parent
        if(!bgState) {
            if(wait(NULL) == -1) {
                //ERROR ERROR ERROR
                //Waiting error
            }
        }
    }
    return 0;
}

int pipedCommand(int count, char** arglist, int pipeIdx) {
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
    int flagFast = strcmp(arglist[count-1],"&")==0; //flagFast is 1 if we dont wait, 0 if we do
    int flagPipe = 0;
    int flagRedirectInput = 0;
    int flagRedirectOutput = 0;
    int idx = elementFinder(arglist,"|");
    if(idx >= 0) {
        flagPipe = idx;
    }
    if(flagPipe) {
        pipedCommand(count,arglist,flagPipe);
        return 1;
    }
    if(flagFast) {
        char** passed;
        int cntCmd = count;
        passed = malloc((count-1)*sizeof(char*));
        for(int i = 0; i < count-1; i++) {
            passed[i] = arglist[i];
        }
        passed[count-1] = arglist[count];
        cntCmd--;
        singleCommand(cntCmd,passed,flagFast);
        free(passed);
        return 1;
    }

    singleCommand(count,arglist,flagFast);
    return 1;
}
int prepare(void) {
    return 0;
}
int finalize(void) {
    return 0;
}


*/
/* 
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
*/
