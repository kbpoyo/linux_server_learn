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
#include "http_parse.hpp"

using namespace http_req_space;

#define BUFFER_SIZE 1024
const char * ip = "172.26.132.79";
const int port = 12345;
const int backlog = 5;

int main(int argc, char* argv[]) {
    

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

     //允许重用本地地址
    int optval = 1;
    int ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    assert(ret != -1);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    

    ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sock, backlog);
    assert(ret != -1);


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
        assert(http.http_read(connfd));

        printf ("http: \n");
        printf ("\tMethod: %d\n", http.method);
        printf ("\turl: %s\n", http.url);
        printf ("\tHost: %s\n", http.host);
        printf ("\tConnection: %s\n", http.connect ? "keep-alive" : "close");
        printf ("\tUser-Agent: %s\n", http.user_agent);
        printf ("\tAccept: %s\n", http.accept);
        printf ("\tAccept-Encoding: %s\n", http.accept_encoding);
        printf ("\tAccept-Language: %s\n", http.accept_language);
        printf ("\tCookie: %s\n", http.cookie);
        printf ("\tReferer: %s\n", http.referer);
        printf ("\tContent-Length: %d\n", http.content_length);
        printf ("\tContent: %s\n", http.content);

        http.http_rep_status_line(404, error_404_title);
        http.http_rep_headers(strlen(error_404_form));
        http.http_rep_content(error_404_form);

        send(connfd, http.write_buffer, http.write_index, 0);

        // sleep(20);

        close(connfd);
    }

    
    close(sock);
    return 0;
}