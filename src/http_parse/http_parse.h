#ifndef HTTP_PARSE_H
#define HTTP_PARSE_H

typedef enum _HTTP_METHOD {
    GET = 0,
    POST,
    PUSH,
    DELETE,
    HEAD,
    PUT
}HTTP_METHOD;

typedef enum _HTTP_CONNECTION {
    KEEP_ALIVE = 0,
    CLOSE
}HTTP_CONNECTION;

#define HTTP_BUFFER_SIZE 128

typedef struct _http_req_t{
    HTTP_METHOD method;
    char url[HTTP_BUFFER_SIZE];
    char referer[HTTP_BUFFER_SIZE];
    char host[HTTP_BUFFER_SIZE / 2];
    HTTP_CONNECTION connection;
    char user_agent[HTTP_BUFFER_SIZE];
    char accept[HTTP_BUFFER_SIZE];
    char accept_encoding[HTTP_BUFFER_SIZE / 2];
    char accept_language[HTTP_BUFFER_SIZE / 2];
    char cookie[HTTP_BUFFER_SIZE];

}http_req_t;

int http_parse(int connfd, http_req_t* http);



#endif