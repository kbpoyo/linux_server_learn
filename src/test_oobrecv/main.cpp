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
    

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

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
        ret = recv(connfd, buffer, BUF_SIZE - 1, 0);
        printf("get %d bytes of normal data '%s'\n", ret, buffer);
        
        memset(buffer, 0, BUF_SIZE);
        ret = recv(connfd, buffer, BUF_SIZE - 1, MSG_OOB);
        printf("get %d bytes of oob data '%s'\n", ret, buffer);
        
        memset(buffer, 0, BUF_SIZE);
        ret = recv(connfd, buffer, BUF_SIZE - 1, 0);
        printf("get %d bytes of normal data '%s'\n", ret, buffer);

    
        // memset(buffer, 0, BUF_SIZE);
        // ret = recv(connfd, buffer, BUF_SIZE - 1, MSG_OOB);
        // printf("get %d bytes of oob data '%s'\n", ret, buffer);
        
        close(connfd);

    }

    
    close(sock);
    return 0;
}