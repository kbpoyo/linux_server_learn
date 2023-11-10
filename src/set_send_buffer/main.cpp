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

#define BUF_SIZE 512

const char * ip = "172.26.132.79";
const int port = 12345;
const int backlog = 5;


int main(int argc, char* argv[]) {
    
    if (argc < 2) {
        printf("usage: %s send_buffer_size\n", basename(argv[0]));
        return 1;
    }

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);


    int sendbuf = atoi(argv[1]);
    int len = sizeof(sendbuf);

    //设置并立刻读取socket选项
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendbuf, len);
    getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendbuf, (socklen_t*)&len);
    printf("the tcp send buffer size after set is %d, sizeof sendbuf = %d\n", sendbuf, len);

    if (connect(sock, (struct sockaddr*)&address, sizeof(address)) != -1) {
        char buffer[BUF_SIZE];
        memset(buffer, 'a', BUF_SIZE);
        send(sock, buffer, BUF_SIZE, 0);
    }

    
    close(sock);
    return 0;
}