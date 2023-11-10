#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define BUF_SIZE 1024

const char * ip = "172.26.132.79";
const int port = 12345;
const int backlog = 5;


int main(int argc, char* argv[]) {
    
      
    if (argc < 2) {
        printf("usage: %s recv_buffer_size\n", basename(argv[0]));
        return 1;
    }


    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int recvbuf = atoi(argv[1]);
    int len = sizeof(recvbuf);
    //设置并立刻获取socket选项
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recvbuf, len);
    getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recvbuf, (socklen_t*) &len);
    printf("the tcp recv buffer size after set is %d, sizeof recvbuf = %d\n", recvbuf, len);

    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sock, backlog);
    assert(ret != -1);


    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
    if (connfd < 0) {
        printf("errno is: %d\n", errno);
    } else {
        char remote[INET_ADDRSTRLEN];
        printf("connected with ip: %s and port: %d\n", 
            inet_ntop(AF_INET, &client.sin_addr, remote, INET_ADDRSTRLEN), 
            ntohs(client.sin_port));

        char buffer[BUF_SIZE];
        memset(buffer, 0, BUF_SIZE);
        while (ret = recv(connfd, buffer, BUF_SIZE - 1, 0) > 0) {
            printf("buf: = %s\n", buffer);
        }
        
        
        close(connfd);

    }

    
    close(sock);
    return 0;
}