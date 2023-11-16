#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/fcntl.h>
#include <sys/signal.h>
#include <errno.h>
#include <sys/epoll.h>


int time_out_connect(const char *ip, int port, int time) {
    int ret = 0;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    struct timeval timeout;
    timeout.tv_sec = time;
    timeout.tv_usec = 0;

    socklen_t len = sizeof(timeout);
    ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
    assert(ret != -1);

    ret = connect(sockfd, (sockaddr*)&address, sizeof(address));
    if (ret == -1) {
        if (errno == EINPROGRESS) { //连接超时
            printf("connecting timeout, process timeout logic\n");
            return -1;
        }

        printf("error occur when connecting to server\n");
        return -1;
    }

    return sockfd;

}


int main(int argc, char *argv[]) {
    if (argc <= 2) {
        printf("useage: %s ip_addr port_number\n", basename(argv[0]));
        return 1;
    }

    const char *ip = argv[1];
    const int port = atoi(argv[2]);
    
    int sockfd = time_out_connect(ip, port, 10);
    if (sockfd < 0) {
        return -1;
    }

    return 0;
}