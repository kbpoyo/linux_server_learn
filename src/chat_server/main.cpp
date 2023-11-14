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
#include <sys/epoll.h>
#include <sys/fcntl.h>

#define BUFFER_SIZE 10
#define MAX_EVENT_NUMBER 65535
#define USER_LIMIT 10

int fds[USER_LIMIT];
int user_count = 0;

const char *ip = "172.26.132.79";
const int port = 12345;

typedef struct _client_data_t {
    sockaddr_in address;
    char *write_buf;
    char buf[BUFFER_SIZE * 10];
}client_data_t;


int setnonblocking(int fd) {
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);

    return old_opt;
}

void addfd(int epollfd, int fd, bool enable_onshot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP;
    if (enable_onshot) {
        event.events |= EPOLLONESHOT;
    }

    setnonblocking(fd);
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}

void resetonshot(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void delete_user(int fd) {
    //加锁
    user_count--;
    for (int i = 0; i < user_count; i++) {
        if (fds[i] == fd) {
            fds[i] = fds[user_count];
            break;
        }
    }
    //解锁
}


int main(int argc, char* argv[]) {

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    //允许重用本地地址
    int optval = 1;
    int ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    assert(ret != -1);

    ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 10);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(USER_LIMIT);
    assert(epollfd >= 0);

    addfd(epollfd, listenfd, false);


    client_data_t *users = new client_data_t[MAX_EVENT_NUMBER];

    while (1) {
        int event_num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (event_num < 0) {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < event_num; i++) {
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
                        printf("accept failure errno is: %d\n", errno);
                        continue;
                    }

                    if (user_count >= USER_LIMIT) {
                        const char* info = "too many users\n";
                        printf("%s", info);
                        send(connfd, info, strlen(info), 0);
                        close(connfd);
                        continue;
                    }

                    //加锁
                    fds[user_count++] = connfd;
                    //解锁
                    users[connfd].address = client_addr;
                    users[connfd].write_buf = NULL;
                    addfd(epollfd, connfd, true);
                    printf("comes a new user, now have %d users\n", user_count);

                }
                
            } else if (events[i].events & EPOLLERR) {
                //获取并清除对端socket错误
                printf("get an error from %d\n", sockfd);
                char errors[100];
                memset(errors, 0, 100);
                socklen_t length = sizeof(errors);
                if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0) {
                    printf("get socket option failed\n");
                }

                printf("get sockerror: '%s' from %d\n", errors, sockfd);

                resetonshot(epollfd, sockfd);
                continue;   
            } else if (events[i].events & EPOLLRDHUP) {
                //多端socket关闭连接，删除本地用户
                epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL);
                close(sockfd);

                delete_user(sockfd);
                
                printf("a client left\n");
                continue;
            } else if (events[i].events & EPOLLIN) {
                memset(users[sockfd].buf, '\0', BUFFER_SIZE * 10);

                int len = 0;
                while (1) {

                    ret = recv(sockfd, users[sockfd].buf + len, BUFFER_SIZE - 1, 0);
                    if (ret < 0) {
                        if (errno == EAGAIN) {  
         
                            resetonshot(epollfd, sockfd);
                            break;

                        }
                        //读出错，关闭连接，删除用户
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL);
                        close(sockfd);

                        delete_user(sockfd);
                            
                        printf("recv error, erron: %d from %d", errno, sockfd);
                        break;
                    } else {
                        //成功收到数据
                        len += ret;
                    }
                }
                printf("get %d bytes of client data %s from %d\n", len, users[sockfd].buf, sockfd);

                //接收完所有数据，通知其它socket准备写数据
                for (int i = 0; i < user_count; i++) {
                    if (fds[i] == sockfd) {
                        continue;
                    }
                    epoll_event event;
                    event.data.fd = fds[i];
                    event.events = EPOLLOUT | EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP | EPOLLONESHOT;
                    epoll_ctl(epollfd, EPOLL_CTL_MOD, fds[i], &event);
                    users[fds[i]].write_buf = users[sockfd].buf;
                }

            } else if (events[i].events & EPOLLOUT) {
                if (!users[sockfd].write_buf) {
                    continue;
                }

                ret = send(sockfd, users[sockfd].write_buf, strlen(users[sockfd].write_buf), 0);
                users[sockfd].write_buf = NULL;
                resetonshot(epollfd, sockfd);
            }
        }
    }

    delete[] users;
    close(listenfd);

    return 0;

}