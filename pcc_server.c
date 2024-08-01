#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

static int signaled = 0; //1 if there exists a current TCP connection,0 otherwise.
//used for signal handling.

void ignHandler() {
    signaled = 1;
}

int main(int argc, char* argv[]){
    if(argc != 2) {
        perror("\n Error due to invalid number of arguments, please pass 1.");
        exit(1);
    }
    unsigned int hist[128] = {0};
    unsigned int pccCounter = 0;

    struct sockaddr_in server = {0};
    struct sockaddr_in client = {0};
    socklen_t addrsize = sizeof(struct sockaddr_in);

    server.sin_family = AF_INET; //we work on IPv4
    server.sin_port = atoi(argv[1]);
    server.sin_addr.s_addr = htonl(INADDR_ANY); //Accept all addresses, .s_addr is long

    int listenfd = socket(AF_INET,SOCK_STREAM,0); //i.e. TCP connection
    int optval = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));

    if(bind(listenfd,(struct sockaddr *)&server,addrsize) != 0) {
        perror("\n Error due binding failiure.");
        exit(1);
    }
    if(listen(listenfd,10) != 0) {
        perror("\n Error due listening failiure.");
        exit(1);
    }

    //Now we entering the loop of TCP connections:
    char buffer[1024] = {0};

    for(;;) {
        if(signaled) {
            for(int b = 32; b <= 126; b++) {
                printf("char '%c' : %u times\n",(char)b,hist[b]);
            }
            exit(0);
        }

        struct sigaction saObedient = {0};
        struct sigaction saDisobedient = {0};

        saObedient.sa_handler = ignHandler;
        saDisobedient.sa_handler = SIG_DFL;

        sigaction(SIGINT,&saDisobedient,NULL);
        int connfd = accept(listenfd,(struct sockaddr*)&client,&addrsize);
        if(connfd < 0) {
            fprintf(stderr,"\n Sorry, an error connecting to server occured.");
            continue;
        }

        long tmp = pccCounter;
        while(int nread = read(connfd,buffer,sizeof(buffer)) > 0) {
            for(int i = 0; i < nread; i++) {
                if(buffer[i] >= 32 && buffer[i] <= 128) {
                    int idx = buffer[i];
                    if(idx > 127) {
                        continue;
                    }
                    hist[idx]++;
                    pccCounter++;
                }
            }
        }

        long delta = pccCounter - tmp;
        char* output = calloc(51,sizeof(char));
        char* tmpPtr = output;
        sprintf(output,"# of printable characters: %ld\n",delta);
        long leftover = strlen(output);
        while(leftover > 0) {
            long nwrite = write(connfd,output,leftover);
            output += nwrite;
            leftover -= nwrite;
        }

        free(tmpPtr);
        sigaction(SIGINT,&saObedient,NULL);
    }


    return 0;
}
