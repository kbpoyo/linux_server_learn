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

#define MAX_EVENT_NUMBER 1024
static int pipefd[2];

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
    event.events = EPOLLIN;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void sig_handler(int sig) {
    //保留原始errno，离开信号处理函数时恢复，以保证函数的线程安全
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

//设置信号处理函数
void addsig(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags = SA_RESTART;
    
    //设置当处理sig信号时，要屏蔽的信号
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

int main(int argc, char* argv[]) {

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

    //创建管道套接字，用于进程内全局通信
    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd >= 0);

    //将读写端设置为非阻塞，并将监听读端
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0]);

    //设置一些信号的处理函数
    addsig(SIGHUP);
    addsig(SIGCHLD);
    addsig(SIGTERM);
    addsig(SIGINT);

    bool stop_server = false;

    while (!stop_server) {

        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            printf("epoll error\n");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;

            if (sockfd == listenfd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                memset(&client_addr, 0, client_len);

                int connfd = accept(listenfd, (sockaddr*)&client_addr, &client_len);
                if (connfd <= 0) {
                    continue;
                }

                addfd(epollfd, connfd);
            } else if (sockfd == pipefd[0] && events[i].events & EPOLLIN) {
                //进程收到信号，逐个读取并分发处理
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                
                if (ret <= 0) {
                    continue;
                }

                for (int i = 0; i < ret; i++) {
                    switch (signals[i]) {
                        case SIGCHLD:
                            printf("recv SIGCHLD\n");
                            break;
                        case SIGHUP:
                            printf("recv SIGHUP\n");
                            break;
                        case SIGTERM:
                            printf("recv SIGTERM\n");
                            break;
                        case SIGINT:
                            printf("recv SIGINT\n");
                            stop_server = true;
                            break;
                        default:
                            break;

                    }
                }

            } else {

            }
        }
    }
    
    printf("close fds\n");
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);

    return 0;
}