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
        close(fd);
        exit(1);
    }
    if(connect(sockfd,(struct sockaddr*)&server,addrsize) != 0) {
        perror("\n Error due connecting failiure.");
        close(sockfd);
        close(fd);
        exit(1);
    }
    char buffer[1024] = {0};

    u_int32_t N = file_stat.st_size;
    long leftover = N;
    N = htonl(N);

    ssize_t bytes_sent = 0;
    ssize_t total_bytes = sizeof(N);

    while (bytes_sent < total_bytes) {
        ssize_t result = write(sockfd, ((char*)&N) + bytes_sent, total_bytes - bytes_sent);
        if (result < 0) {
            perror("\n Error regarding file's size sending ");
            close(sockfd);
            close(fd);
            exit(1);
        }
        bytes_sent += result;
    }

    while(leftover > 0) {
        u_int32_t nread = read(fd,buffer,sizeof(buffer));
        u_int32_t nwrite = write(sockfd,buffer,nread);
        leftover -= nwrite;
    }

    u_int32_t result;
    ssize_t bytes_received = 0;
    total_bytes = sizeof(result);

    while (bytes_received < total_bytes) {
        ssize_t nread = read(sockfd, ((char*)&result) + bytes_received, total_bytes - bytes_received);
        if (nread < 0) {
            perror("An error receiving result occured");
            close(sockfd);
            close(fd);
            exit(1);
        }
        bytes_received += nread;
    }

    result = ntohl(result);
    printf("# of printable characters: %u\n", result);
}
