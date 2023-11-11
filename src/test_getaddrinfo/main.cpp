#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>

const char * ip = "172.26.132.79";
const int port = 12345;

int main(int argc, char* argv[]) {

    assert(argc == 2);
    //获取目标主机地址信息
    char *host = argv[1];
    struct hostent* hostinfo = gethostbyname(host); 
    assert(hostinfo);
    printf("host_name = %s\nhost_aliases: \n", hostinfo->h_name);
    for (char ** aliases = hostinfo->h_aliases; *aliases != NULL; aliases++) {
        printf("\t%s\n", *aliases);
    }
    printf("host_addrtype = %d\n", hostinfo->h_addrtype);
    printf("host_addr_list: \n");
    for (char **addr_list = hostinfo->h_addr_list; *addr_list != NULL; addr_list++) {
        char buf[128];
        printf("\t%s\n", inet_ntop(AF_INET, *addr_list, buf, INET_ADDRSTRLEN));
    }
    

    //获取daytime服务信息
    struct servent* servinfo = getservbyname("daytime", "tcp");
    assert(servinfo);
    printf("server_name = %s\n", servinfo->s_name);
    printf("server_aliases: \n");
    for (char** aliases = servinfo->s_aliases; *aliases != NULL; aliases++) {
        printf("\t%s\n", *aliases);
    }
    printf("daytime port is %d\n", ntohs(servinfo->s_port));
    printf("server_proto = %s\n", servinfo->s_proto);

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = servinfo->s_port;
    address.sin_addr = *(struct in_addr*)(*(hostinfo->h_addr_list));

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int result = connect(sockfd, (struct sockaddr*)&address, sizeof(address));
    assert(result != -1);

    char buffer[128];
    // result = recv(sockfd, buffer, sizeof(buffer), 0);
    result = read(sockfd, buffer, sizeof(buffer));
    assert(result > 0);
    buffer[result] = '\0';
    printf("the day time is: %s", buffer);
    close(sockfd);


   return 0;
}