#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

static volatile int signaled = 0;

u_int32_t hist[128] = {0};
u_int32_t pccCounter = 0;

void obeyHandler() {
    for (int b = 32; b <= 126; b++) {
        printf("char '%c' : %u times\n", (char)b, hist[b]);
    }
    exit(0);
}

void ignHandler() {
    signaled = 1;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "\n Error - Invalid number of arguments .\n");
        exit(1);
    }

    struct sockaddr_in server = {0};
    struct sockaddr_in client = {0};
    socklen_t addrsize = sizeof(struct sockaddr_in);

    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[1]));
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(listenfd, (struct sockaddr*)&server, addrsize) != 0) {
        perror("\nError - Binding failed.");
        exit(1);
    }
    if (listen(listenfd, 10) != 0) {
        perror("\nError - Listening failed.");
        exit(1);
    }

    struct sigaction saIgn = {0};
    saIgn.sa_handler = ignHandler;

    struct sigaction saObey = {0};
    saIgn.sa_handler = obeyHandler;

    char buffer[1024];

    for (;;) {
        if (signaled) {
            //obeyHandler();
            for (int b = 32; b <= 126; b++) {
                printf("char '%c' : %u times\n", (char)b, hist[b]);
            }
            exit(0);
        }

        int connfd = accept(listenfd, (struct sockaddr*)&client, &addrsize);
        if (connfd < 0) {
            perror("\nError - Accepting connection failed.");
            continue;
        }
        sigaction(SIGINT, &saIgn, NULL);

        u_int32_t N;
        ssize_t bytes_received = 0;
        ssize_t total_bytes = sizeof(N);
        while (bytes_received < total_bytes) {
            ssize_t nread = read(connfd, ((char*)&N) + bytes_received, total_bytes - bytes_received);
            if (nread < 0) {
                perror("Error reading N");
                close(connfd);
                continue;
            }
            bytes_received += nread;
        }
        N = ntohl(N);

        u_int32_t tmp = pccCounter;
        u_int32_t bytes_left = N;

        while (bytes_left > 0) {
            ssize_t nread = read(connfd, buffer, sizeof(buffer));
            if (nread < 0) {
                perror("Error reading data");
                break;
            }
            for (ssize_t i = 0; i < nread; i++) {
                if (buffer[i] >= 32 && buffer[i] <= 126) {
                    hist[(unsigned char)buffer[i]]++;
                    pccCounter++;
                }
            }
            bytes_left -= nread;
        }

        u_int32_t delta = pccCounter - tmp;
        delta = htonl(delta);

        ssize_t bytes_sent = 0;
        total_bytes = sizeof(delta);
        while (bytes_sent < total_bytes) {
            ssize_t nwrite = write(connfd, ((char*)&delta) + bytes_sent, total_bytes - bytes_sent);
            if (nwrite < 0) {
                perror("Error writing result");
                break;
            }
            bytes_sent += nwrite;
        }

        close(connfd);
        sigaction(SIGINT, &saObey, NULL);
    }
}
