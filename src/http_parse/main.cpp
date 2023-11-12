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
#include "http_parse.h"

#define BUFFER_SIZE 1024
const char * ip = "172.26.132.79";
const int port = 12345;
const int backlog = 5;

int main(int argc, char* argv[]) {
    

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sock, backlog);
    assert(ret != -1);

    sleep(20);

    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
    if (connfd < 0) {
        printf("errno is: %d\n", errno);
    } else {
        char remote[INET_ADDRSTRLEN];
        printf("connected with ip: %s and port: %d\n", 
            inet_ntop(AF_INET, &client.sin_addr, remote, INET_ADDRSTRLEN), 
            ntohs(client.sin_port));

        char buf[BUFFER_SIZE];
        int len = 0;
        
        http_req_t http;
        ret = http_parse(connfd, &http);
        assert(ret != -1);

        printf ("http: \n");
        printf ("\tMethod: %d\n", http.method);
        printf ("\turl: %s\n", http.url);
        printf ("\tHost: %s\n", http.host);
        printf ("\tConnection: %s\n", http.connection == KEEP_ALIVE ? "keep-alive" : "close");
        printf ("\tUser-Agent: %s\n", http.user_agent);
        printf ("\tAccept: %s\n", http.accept);
        printf ("\tAccept-Encoding: %s\n", http.accept_encoding);
        printf ("\tAccept-Language: %s\n", http.accept_language);
        printf ("\tCookie: %s\n", http.cookie);
        printf ("\tReferer: %s\n", http.referer);

        printf("over");
        send(connfd, "Hello world\n", 12, 0);

        close(connfd);
    }

    
    close(sock);
    return 0;
}