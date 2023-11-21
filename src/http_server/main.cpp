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
#include <sys/signal.h>
#include <sys/epoll.h>
#include "http_conn.hpp"
#include "thread_pool.hpp"

using namespace http_req_space;
using namespace http_conn_space;
using namespace thread_space;

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

const char * ip = "172.26.132.79";
const int port = 12345;
const int backlog = 5;

void addsig(int sig, void(handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }

    sigfillset(&sa.sa_mask);

    assert(sigaction(sig, &sa, NULL) != -1);
}

void show_error(int connfd, const char *info) {
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}


int main(int argc, char* argv[]) {
    
    //忽略SIGPIPE信号
    addsig(SIGPIPE, SIG_IGN);

    //创建线程池
    thread_pool<http_conn>* pool = NULL;

    try {

        pool = new thread_pool<http_conn>;
    } catch (...) {
        return 1;
    }

    //预先为每一个客户连接分配一个http_conn对像
    http_conn* users = new http_conn[MAX_FD];
    assert(users);
    int user_count = 0;

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    struct linger tmp = {1 , 0};
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

     //允许重用本地地址
    int optval = 1;
    int ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    assert(ret != -1);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, backlog);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    while (true) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            printf("epoll error, errno is: %d\n", errno);
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;

            if (sockfd == listenfd) {
                struct sockaddr_in client_address;
                socklen_t client_len = sizeof(client_address);

                int connfd = accept(listenfd, (sockaddr*)&client_address, &client_len);
                if (errno == EAGAIN) {
                    printf("eagain was set in accept\n");
                }
                if (connfd < 0) {
                    printf("accept error, errno is: %d\n", errno);
                    continue;
                }

                if (http_conn::m_user_count >= MAX_FD) {
                    show_error(connfd, "Internal server busy");
                    continue;
                }

                //初始化客户连接
                users[connfd].init(connfd, client_address);

                printf("accept socketfd %d\n", connfd);
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                users[sockfd].close_conn();
            } else if (events[i].events & EPOLLIN) {
                printf("sockfd %d start to read\n", sockfd);
                if (users[sockfd].read()) {
                    //该连接读取成功，将连接任务添加到线程池
                    pool->append(users + sockfd);
                } else {
                    users[sockfd].close_conn();
                }
            } else if (events[i].events & EPOLLOUT) {
                printf("sockfd %d start to write\n", sockfd);
                if (!users[sockfd].write()) {
                    users[sockfd].close_conn();
                }
            }
        }
    }
        

    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}