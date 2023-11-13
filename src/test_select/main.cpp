#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

const char *ip = "172.26.132.79";
const int port = 12345;


int main (int argc, char* argv[]) {
    
    int ret = 0;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));    
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd != -1);

    ret = bind(sockfd, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sockfd, 5);
    assert(ret != -1);

    struct sockaddr_in client_address;
    memset(&client_address, 0, sizeof(client_address));
    socklen_t client_addrlength = sizeof(client_address);
    int connfd = accept(sockfd, (sockaddr*)&client_address, &client_addrlength);

    if (connfd < 0) {
        printf("errno is: %d\n", errno);
        close(sockfd);
    }

    char buf[BUFFER_SIZE];
    fd_set read_fds;
    fd_set exception_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&exception_fds);

    while (1) {
        memset(buf, '\0', sizeof(buf));

        //每次调用select前都要重新在文件描述符集合中设置文件描述符
        //因为事件发生之后，文件描述符将被内核修改
        FD_SET(connfd, &read_fds);
        FD_SET(connfd, &exception_fds);
        ret = select(connfd + 1, &read_fds, NULL, &exception_fds, NULL);
        if (ret < 0) {
            printf("selection failure\n");
            break;
        }

        if (FD_ISSET(connfd, &read_fds)) {
            ret = recv(connfd, buf, sizeof(buf) - 1, 0);
            if (ret <= 0) {
                break;
            }

            printf("get %d bytes of normal data: %s\n", ret, buf);
        }
        
        if (FD_ISSET(connfd, &exception_fds)) {

            memset(buf, '\0', sizeof(buf));
            ret = recv(connfd, buf, sizeof(buf) - 1, MSG_OOB);
            if (ret <= 0) {
                break;
            }

            printf("get %d bytes of oob data: %s\n", ret, buf);
        }

    }

    close(connfd);
    close(sockfd);

    return 0;
}