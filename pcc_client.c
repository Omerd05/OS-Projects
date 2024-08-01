#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>

int main(int argc, char* argv[]) {
    if(argc != 4) {
        perror("\n Error due to invalid number of arguments, please pass 1.");
        exit(1);
    }
    struct stat file_stat;
    int fd = open(argv[3],O_RDONLY);
    fstat(fd,&file_stat);

    struct sockaddr_in client = {0};
    struct sockaddr_in server = {0};
    socklen_t addrsize = sizeof(client);

    client.sin_family = AF_INET;

    server.sin_family = AF_INET; //Again,we work on IPv4
    inet_pton(AF_INET,argv[1],&server.sin_addr.s_addr);
    server.sin_port = htons(atoi(argv[2]));

    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(bind(sockfd,(struct sockaddr*)&client,addrsize) != 0) {
        perror("\n Error due binding failiure.");
        exit(1);
    }
    if(connect(sockfd,(struct sockaddr*)&server,addrsize) != 0) {
        perror("\n Error due connecting failiure.");
        exit(1);
    }
    char buffer[1024] = {0};

    u_int32_t N = file_stat.st_size;
    long leftover = N;
    N = htonl(N);
    long ones = pow(2,8) - 1;

    for(int i = 0; i < 4; i++) {
        char currByte = N & (ones << 8*i);
        write(sockfd,&currByte,sizeof(currByte));
    }

    while(leftover > 0) {
        u_int32_t nread = read(fd,buffer,sizeof(buffer));
        u_int32_t nwrite = write(sockfd,buffer,nread);
        leftover -= nwrite;
    }

    char input[50] = {0};
    char* tmp = input;
    while(u_int32_t nread = read(sockfd,tmp,sizeof(tmp)) > 0) {
        for(int i = 0; i < nread; i++) {
            tmp[i] = ntohl(tmp[i]);
        }
        tmp += nread;
    }
    u_int32_t result = 0;
    for(int i = 0; i < (tmp - input); i++) {
        result |= input[i] << 8*i;
    }
    printf("# of printable characters: %u\n",result);
    return 0;
}
