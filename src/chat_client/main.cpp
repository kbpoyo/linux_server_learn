#define _GNU_SOURCE 1
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
// #include <sys/types.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/fcntl.h>


#define BUFFER_SIZE 64

int main(int argc, char* argv[]) {

    if (argc <= 2) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    if (connect(sockfd, (sockaddr*)&address, sizeof(address)) < 0) {
        printf("connection failed\n");
        close(sockfd);
        return 1;
    }

    //设置poll的监听描述符集
    pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN; //监听终端输入事件
    fds[0].revents = 0;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP; //监听socket读事件和对端关闭事件
    fds[1].revents = 0;

    char read_buf[BUFFER_SIZE];

    //创建管道，将终端输入与socket输出连接起来
    int pipefd[2];
    int ret = pipe(pipefd);
    assert(ret != -1);

    while (1) {
        ret = poll(fds, 2, -1);

        if (ret < 0) {
            printf("poll failure\n");
            break;
        }

        if (fds[1].revents & POLLRDHUP) {
            printf("server close the connection\n");
            break;
        } else if (fds[1].revents & POLLIN) {
            memset(read_buf, 0, BUFFER_SIZE);
            recv(fds[1].fd, read_buf, BUFFER_SIZE - 1, 0);
            printf(">>> %s\n", read_buf);
        }

        if (fds[0].revents & POLLIN) {
            ret = splice(STDIN_FILENO, NULL, pipefd[1], NULL, 32768,
                        SPLICE_F_MORE | SPLICE_F_MOVE);

            assert(ret != -1);
            
            ret = splice(pipefd[0], NULL, sockfd, NULL, 32768,
                        SPLICE_F_MORE | SPLICE_F_MOVE);

            assert(ret != -1);
        }

    }

    
    close(sockfd);
    return 0;
}