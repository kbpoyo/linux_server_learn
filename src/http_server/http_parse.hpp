#ifndef HTTP_PARSE_HPP
#define HTTP_PARSE_HPP

#include <string>
#include <cstring>

#define HTTP_BUFFER_SIZE 4096
//主状态机的两种可能状态
typedef enum _CHECK_STATE {
    CHECK_STATE_REQUESTLINE = 0,    //分析请求行
    CHECK_STATE_HEADER  //分析请求头
}CHECK_STATE;

//从状态机的三种可能状态
typedef enum _LINE_STATUS {
    LINE_OK = 0,    //读取到一个完整行
    LINE_BAD,   //行出错
    LINE_OPEN   //行数据尚不完整
}LINE_STATUS;

//处理http请求的结果
typedef enum _HTTP_CODE {
    NO_REQUEST = 0, //请求不完整
    GET_REQUEST,    //获取一个完整的客户请求
    BAD_REQUEST,    //客户请求有语法错误
    FORBIDDEN_REQUEST,  //客户端对资源没有足够的访问权限
    INTERNAL_ERROR,     //服务器内部错误
    CLOSED_CONNECTION   //客户端已关闭连接
}HTTP_CODE;


class http_req_t{

public:
    http_req_t():check_state(CHECK_STATE_REQUESTLINE){};
    ~http_req_t(){};

    bool http_parse(int connfd);

private:

    LINE_STATUS parse_line();
    HTTP_CODE parse_requestline(char *temp);
    HTTP_CODE parse_headers(char *temp);
    HTTP_CODE parse_content();




public:
    const char* method{};
    const char* url{};
    const char* referer{};
    const char* host{};
    bool connect{};
    const char* user_agent{};
    const char* accept{};
    const char* accept_encoding{};
    const char* accept_language{};
    const char* cookie{};
    const char* content{};
    unsigned int content_length{};
    
    //http连接的读缓冲区，及其状态参数
    char read_buffer[HTTP_BUFFER_SIZE]{};
    unsigned int checked_index{};
    unsigned int read_index{};
    unsigned int start_line{};

    //http连接的写缓冲区，及其状态参数
    char write_buffer[HTTP_BUFFER_SIZE];
    unsigned int write_index;

    //主机当前所处状态
    CHECK_STATE check_state{};

};




/**
 * @brief 从状态机，用于解析出一行数据
 * 
 * @param buffer http请求头的缓冲区
 * @param checked_index 当前解析的字符位置
 * @param read_index http请求头已被读取到缓冲区的有效字符的末尾位置
 * @return LINE_STATUS 当前解析结果状态
 */
LINE_STATUS http_req_t::parse_line() {
    char temp;
    for (; checked_index < read_index; ++checked_index) {
        temp = read_buffer[checked_index];

        //当前字符为'\r'，说明可能读到一个完整的行
        if (temp == '\r') {
            if (checked_index + 1 == read_index) {
                return LINE_OPEN;
            } else if (read_buffer[checked_index + 1] == '\n') {
                read_buffer[checked_index++] = '\0';
                read_buffer[checked_index++] = '\0';
                return LINE_OK;
            }

            return LINE_BAD;
        } else if (temp == '\n') {
            if (checked_index > 0 && read_buffer[checked_index - 1] == '\r') {
                read_buffer[checked_index - 1] = '\0';
                read_buffer[checked_index++] = '\0';
                return LINE_OK;
            }

            return LINE_BAD;
        }
    }

    return LINE_OPEN;
}


/**
 * @brief 解析请求行
 * 
 * @param temp 请求行在缓冲区的地址
 * @param check_state 主状态机的当前状态
 * @param http 将解析结果放入该http结构中
 * @return HTTP_CODE 解析的结果状态
 */
HTTP_CODE http_req_t::parse_requestline(char *temp) {
    //将temp截断为请求行的方法字段

    char *url = strpbrk(temp, " \t");
    if (!url) {
        return BAD_REQUEST;
    }
    *(url++) = '\0';
    char *method = temp;

    //解析方法
    if (strlen(method) <= 0) {
        return BAD_REQUEST;
    }
    this->method = method;

    //将url截断为请求行的url
    url += strspn(url, " \t");

    //将version截断为请求行的版本
    char *version = strpbrk(url, " \t");
    if (!version) {
        return BAD_REQUEST;
    }
    *(version++) = '\0';
    version += strspn(version, " \t");

    //仅支持 HTTP/1.1
    if (strcasecmp(version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }

    //检查url是否合法
    if (strncasecmp(url, "http://", 7) == 0) {
        url += 7;
        url = strchr(url, '/');
    }

    if (!url || url[0] != '/') {
        return BAD_REQUEST;
    }

    this->url = url;
    check_state = CHECK_STATE_HEADER;
    
    return NO_REQUEST;
}


/**
 * @brief 解析请求头
 * 
 * @param temp 该行在缓冲区的位置
 * @param http 将结果解析到该http结构中
 * @return HTTP_CODE 解析http的结果状态
 */
HTTP_CODE http_req_t::parse_headers(char *temp) {
    //遇到空行，说明得到了一个正确的http请求
    if (temp[0] == '\0') {
        return GET_REQUEST;
    } else if (strncasecmp(temp, "Host:", 5) == 0) {
        temp += 5;
        temp += strspn(temp, " \t");
        this->host = temp;
    } else if (strncasecmp(temp, "Connection:", 11) == 0) {
        temp += 11;
        temp += strspn(temp, " \t");
        this->connect = strcmp(temp, "close") ? true : false;
    } else if (strncasecmp(temp, "User-Agent:", 11) == 0) {
        temp += 11;
        temp += strspn(temp, " \t");
        this->user_agent = temp;
    } else if (strncasecmp(temp, "Accept:", 7) == 0) {
        temp += 7;
        temp += strspn(temp, " \t");
        this->accept = temp;
    } else if (strncasecmp(temp, "Accept-Encoding:", 16) == 0) {
        temp += 16;
        temp += strspn(temp, " \t");
        this->accept_encoding = temp;
    } else if (strncasecmp(temp, "Accept-Language:", 16) == 0) {
        temp += 16;
        temp += strspn(temp, " \t");
        this->accept_language = temp;
    } else if (strncasecmp(temp, "Cookie:", 7) == 0) {
        temp += 7;
        temp += strspn(temp, " \t");
        this->cookie = temp;
    } else if (strncasecmp(temp, "Referer:", 8) == 0) {
        temp += 8;
        temp += strspn(temp, " \t");
        this->referer = temp;
    } else if (strncasecmp(temp, "Content-Length:", 15) == 0) {
        temp += 15;
        temp += strspn(temp, " \t");
        this->content_length = atoi(temp);
    }

    return NO_REQUEST;
}

/**
 * @brief 解析http请求的内容
 * 
 * @param buffer 请求内容缓冲区
 * @param checked_index 当前解析的字符位置
 * @param read_index 缓冲区中最后一个有效字符的下一个位置
 * @param start_line 当前解析行的起始位置
 * @param check_state 当前状态
 * @param http 解析结果到该结构
 * @return HTTP_CODE http请求的结果状态
 */
HTTP_CODE http_req_t::parse_content() {

        LINE_STATUS line_status = LINE_OK;
        HTTP_CODE retcode = NO_REQUEST;

        //主状态机，用于从buffer中取出所有完整的行

        while ((line_status = parse_line()) == LINE_OK) {
            char *temp = read_buffer + start_line;
            start_line = checked_index;

            switch (check_state) {
            case CHECK_STATE_REQUESTLINE: { //分析请求行
                retcode = parse_requestline(temp);
                if (retcode == BAD_REQUEST) {
                    return retcode;
                }
            } break;

            case CHECK_STATE_HEADER: {  //分析请求头
                retcode = parse_headers(temp);
                if (retcode == BAD_REQUEST || retcode == GET_REQUEST) {
                    return retcode;
                } 
            } break;
            
            default:
                return INTERNAL_ERROR;
            }
        }

        if (line_status == LINE_OPEN) {
            return NO_REQUEST;
        } else {
            return BAD_REQUEST;
        }
}


/**
 * @brief 解析http请求
 * 
 * @param connfd 已连接的socket文件描述符
 * @param http 解析到该http结构中
 * @return int 解析结果
 */
bool http_req_t::http_parse(int connfd) {
    memset(read_buffer, '\0', HTTP_BUFFER_SIZE);
    int data_read = 0;

    while (1) {
        data_read = recv(connfd, read_buffer + read_index, HTTP_BUFFER_SIZE - read_index, 0);
        if (data_read == -1) {
            return false;
        } else if (data_read == 0) {
            return false;
        }

        read_index += data_read;
        HTTP_CODE result = parse_content();

        if (result == NO_REQUEST) {
            continue;
        } else if (result == GET_REQUEST) {
            return true;
        }

        return false;
    }
}

#endif