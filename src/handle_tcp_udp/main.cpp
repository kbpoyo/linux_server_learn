#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/epoll.h>

#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

const char *ip = "172.26.132.79";
const int port = 12345;

int setnonblocking(int fd) {
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);

    return old_opt;
}


void addfd(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}


int main(int argc, char* argv[]) {

    //创建tcp监听套接字
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    int optval = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    assert(ret != -1);

    //创建udp套接字
    int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    ret = bind(udpfd, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    //创建epoll进行多路IO监听
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd >= 0);

    //注册tcp和udp上的可读事件
    addfd(epollfd, listenfd);
    addfd(epollfd, udpfd);


    while (1) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if (number < 0) {
            printf("epoll wait error\n");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;

            if (sockfd == listenfd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);

                while (1) {
                    memset(&client_addr, 0, client_len);

                    int connfd = accept(listenfd, (sockaddr*)&client_addr, &client_len);

                    if (connfd < 0) {
                        if (errno == EAGAIN) {
                            break;
                        }

                        printf("accpet error\n");
                        break;
                    }

                    addfd(epollfd, connfd);
                }
            } else if (sockfd == udpfd) {
                char buf[UDP_BUFFER_SIZE];

                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);

                int len = 0;

                while (1) {
                    memset(buf, '\0', UDP_BUFFER_SIZE);
                    ret = recvfrom(udpfd, buf, UDP_BUFFER_SIZE - 1, 0, (sockaddr*)&client_addr, &client_len);

                    if (ret < 0) {
                        if (errno != EAGAIN) {
                            close(udpfd);
                            printf("recvfrom error\n");
                        }

                        break;
                    } else if (ret > 0) {
                        sendto(udpfd, buf, ret, 0, (sockaddr*)&client_addr, client_len);
                    }

                }
            } else if (events[i].events & EPOLLIN) {
                char buf[TCP_BUFFER_SIZE];

                while (1) {
                    memset(buf, '\0', TCP_BUFFER_SIZE);
                    ret = recv(sockfd, buf, TCP_BUFFER_SIZE - 1, 0);

                    if (ret < 0) {
                        if (errno != EAGAIN) {
                            close(sockfd);
                            printf("recv error\n");
                        }

                        break;
                    } else if (ret == 0) {
                            close(sockfd);
                    } else {
                        send(sockfd, buf, ret, 0);
                    }
                }
            } else {
                printf("something else happened\n");
            }
        }


    }
    
    close(listenfd);
    close(udpfd);
    return 0;
}