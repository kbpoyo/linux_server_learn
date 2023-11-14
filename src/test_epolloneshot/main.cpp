#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <pthread.h>
#include <sys/syscall.h> /*必须引用这个文件 */
 
pid_t gettid(void)
{
	return syscall(SYS_gettid);
}


#define BUFFER_SIZE 10
#define MAX_EVENT_NUMBER 1024

const char *ip = "172.26.132.79";
const int port = 12345;

typedef struct _fds_t{
    int sockfd;
    int epollfd;
}fds_t;

int setnonblocking(int fd) {
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return new_opt;
}

void addfd(int epollfd, int fd, bool enable_oneshot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if (enable_oneshot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void reset_oneshot(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void* worker(void * fds) {
    int sockfd = ((fds_t*)fds)->sockfd;
    int epollfd = ((fds_t*)fds)->epollfd;

    printf("start new thread to receive data on fd: %d\n", sockfd);
    char buf[BUFFER_SIZE];
    memset(buf, '\0', BUFFER_SIZE);

    //循环读取sockfd上的数据，直到遇到EAGAIN错误
    while (1) {

        int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
        if (ret == 0) {
            close(sockfd);
            printf("foreiner closed the connection\n");
            break;
        } else if (ret < 0) {
            if (errno == EAGAIN) {
                reset_oneshot(epollfd, sockfd);
                printf("read later\n");
                break;
            }

            printf("error: %d happened\n", errno);
            break;
        } else {
            printf("thread %d get content: %s\n",  gettid(), buf);
            sleep(10);
        }
    }

    printf("end thread receiving data on fd: %d\n", sockfd);   

    return 0;
}

/**
 * 在ET模式下，对方连接写入一次数据，就会触发一次读事件，若当前处理线程仍在处理该socket时
 * 对方连接写入数据，再次触发读事件，将会有另一个线程同时获得该socket进行处理，这样就有可能多个线程处理一个socket
 * 
 * 为了使一个socket同一时间只能由一个线程处理，所以需要设置ONESHOT模式，
 * 该模式下事件只会被触发一次，直到该事件再次被注册才能被再次触发
 */

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

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd >= 0);

    addfd(epollfd, listenfd, false);

    while (1) {

        ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        for (int i = 0; i < ret; i++) {
            int sockfd = events[i].data.fd;

            if (sockfd == listenfd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                memset(&client_addr, 0, client_len);

                int connfd = accept(listenfd, (sockaddr*)&client_addr, &client_len);
                assert(connfd >= 0);

                //对每个非监听文件描述符都注册EPOLLONESHOT事件
                addfd(epollfd, connfd, true);
            } else if (events[i].events & EPOLLIN) {
                pthread_t thread;
                fds_t fds_for_new_worker;
                fds_for_new_worker.epollfd = epollfd;
                fds_for_new_worker.sockfd = sockfd;

                pthread_create(&thread, NULL, worker, (void*)&fds_for_new_worker);
            } else {
                printf("something else happened\n");
            }
        }
    }

    close(listenfd);
    return 0;
}