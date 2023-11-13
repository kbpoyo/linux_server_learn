#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <assert.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <errno.h>

#define BUFFER_SIZE 10
#define MAX_EVENT_NUMBER 1024

const char *ip = "172.26.132.79";
const int port = 12345;



int setnonblocking(int fd) {
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);

    return old_opt;
}

/**
 * @brief 将文件描述符fd上的EPOLLIN注册到epollfd指示的epoll内核事件表中
 *      参数enable_et指定是否对fd启用ET模式
 * 
 * @param epollfd 
 * @param fd 
 * @param enable_et 
 */
void addfd(int epollfd, int fd, bool enable_et) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    
    if (enable_et) {
        event.events |= EPOLLET;
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

/**
 * @brief LT模式的工作流程
 * 
 * @param events 
 * @param number 
 * @param epollfd 
 * @param listenfd 
 */
void lt(epoll_event* events, int number, int epollfd, int listenfd) {
    char buf[BUFFER_SIZE];

    for (int i = 0; i < number; i++) {
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd) {
            struct sockaddr_in client_addr;
            memset(&client_addr, 0, sizeof(client_addr));
            socklen_t client_len = sizeof(client_addr);
            int connfd = accept(listenfd, (sockaddr*)&client_addr, &client_len);
            addfd(epollfd, connfd, false);
        } else if (events[i].events & EPOLLIN) {
            printf("event trigger once\n");
            memset(buf, '\0', BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
            if (ret <= 0) { //读取异常或对端关闭，则关闭该文件描述符
                close(sockfd);
                continue;
            }

            printf("get %d bytes of content: %s\n", ret, buf);
        } else {
            printf("something else happend\n");
        }
 
    }
}


/**
 * @brief ET模式的工作流程
 * 
 * @param events 
 * @param number 
 * @param epollfd 
 * @param listenfd 
 */
void et(epoll_event* events, int number, int epollfd, int listenfd) {
    char buf[BUFFER_SIZE];

    for (int i = 0; i < number; i++) {
        int sockfd = events[i].data.fd;
        
        if (sockfd == listenfd) {    //监听socket触发，说明有外部连接建立
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            memset(&client_addr, 0, client_len);
            int connfd = accept(listenfd, (sockaddr*)&client_addr, &client_len);
            addfd(epollfd, connfd, true);
        } else if (events[i].events & EPOLLIN) {
            //处于ET模式，该事件触发后会被清除，所以不会重复触发，
            //需要确保把socket缓冲区中的所有数据读出 
            printf("event trigger once\n");
            while (1) {
                memset(buf, '\0', BUFFER_SIZE);
                int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
                if (ret < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf("read later\n");
                        break;
                    }

                    close(sockfd);
                    break;
                } else if (ret == 0) {
                    close(sockfd);
                    break;
                } else {
                    printf("get %d bytes of content: %s\n", ret, buf);
                }
            }
            
        } else {
            printf("something else happened\n");
        }
    }
}

/**
 * LT模式下，只要缓冲区中有数据可读，便会在每一次epoll_wait的时候触发事件
 * ET模式下，只有在连接方写入一次数据后，才会触发一次事件，即使缓冲区中仍有数据未被读取
*/

int main(int argc, char* argv[]) {

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd != -1);

    int ret = 0;
    ret = bind(sockfd, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sockfd, 5);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, sockfd, true);

    while (1) {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if (ret < 0) {
            printf ("epoll failure\n");
            break;
        }

        // lt(events, ret, epollfd, sockfd);
        et(events, ret, epollfd, sockfd);
    }
    
    close(sockfd);

    return 0;
}