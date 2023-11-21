#ifndef HTTP_PARSE_HPP
#define HTTP_PARSE_HPP

#include <string>
#include <cstring>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/fcntl.h>

namespace http_req_space {

#define HTTP_READ_BUFFER_SIZE 2048
#define HTTP_WRITE_BUFFER_SIZE 1024
#define FILE_NAME_SIZE 128
//主状态机的两种可能状态
typedef enum _CHECK_STATE {
    CHECK_STATE_REQUESTLINE = 0,    //分析请求行
    CHECK_STATE_HEADER,  //分析请求头
    CHECK_STATE_CONTENT
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
    NO_RESOURCE,    //没有资源
    FORBIDDEN_REQUEST,  //客户端对资源没有足够的访问权限
    FILE_REQUEST,   //文件请求
    INTERNAL_ERROR,     //服务器内部错误
    CLOSED_CONNECTION   //客户端已关闭连接
}HTTP_CODE;

typedef enum _HTTP_METHOD {
    GET = 0,
    POST,
    PUSH,
    DELETE,
    HEAD,
    PUT
}HTTP_METHOD;


//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";;
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file from this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the requested file.\n";

//网站根目录
const char *doc_root = "/var/www/html";

class http_req_t{

public:
    http_req_t():check_state(CHECK_STATE_REQUESTLINE) {
        memset(read_buffer, 0, HTTP_READ_BUFFER_SIZE);
        memset(write_buffer, 0, HTTP_WRITE_BUFFER_SIZE);
    };

    ~http_req_t(){ unmap(); };

    ssize_t http_read(int connfd);
    ssize_t http_write(int connfd);
    bool http_write_buffer(HTTP_CODE ret);
    bool http_is_writebuffer_empty();
    HTTP_CODE http_parse();
    void http_reset();

    bool http_rep_status_line(int status, const char *title);
    bool http_rep_headers(int content_len);
    bool http_rep_content_length(int content_len);
    bool http_rep_is_keepalive();
    bool http_rep_blank_line();
    bool http_rep_content(const char *content);


private:

    LINE_STATUS parse_line();
    HTTP_CODE parse_requestline(char *temp);
    HTTP_CODE parse_headers(char *temp);
    HTTP_CODE parse_content(char *temp);
    HTTP_CODE do_request();
    void unmap();   //释放映射的文件内存

    bool add_response(const char *format, ...);




public:
    HTTP_METHOD method{};
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
    
    //http连接的写缓冲区，及其状态参数
    char write_buffer[HTTP_WRITE_BUFFER_SIZE];
    unsigned int write_index{};


private:   
    //http连接的读缓冲区，及其状态参数
    char read_buffer[HTTP_READ_BUFFER_SIZE]{};
    unsigned int checked_index{};
    unsigned int read_index{};
    unsigned int start_line{};

    //主机当前所处状态
    CHECK_STATE check_state{};

    
    //客户请求的目标文件的完整路径，其内容等于 doc_root + m_url
    char m_real_file[FILE_NAME_SIZE]{};
    char *m_file_address{};   //客户请求的目标文件被mmap到内存的起始位置

    struct stat m_file_stat{};    //目标文件的状态

    //采用writev集中写的方式,将响应头和文件内容一起写回给客户端
    struct iovec m_iv[2]{}; 
    int m_iv_count{};

    unsigned int m_bytes_have_send{};

};

/**
 * @brief 释放映射的文件内存
 * 
 */
void http_req_t::unmap() {
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = NULL;
    }
}

ssize_t http_req_t::http_write(int connfd) {

  int bytes_to_send = 0;
  bytes_to_send = writev(connfd, m_iv, m_iv_count);

  if (bytes_to_send < 0) {

    unmap();
    return bytes_to_send;
  } 

  m_bytes_have_send += bytes_to_send;

  if (http_is_writebuffer_empty()) {
    unmap();
  }

 return bytes_to_send;

}

bool http_req_t::http_is_writebuffer_empty() {
    return m_bytes_have_send >= m_iv[0].iov_len + m_iv[1].iov_len;
}

/**
 * @brief 重置连接
 * 
 */
void http_req_t::http_reset() {
    method = GET;
    url = NULL;
    referer = NULL;
    host = NULL;
    connect = false;
    user_agent = NULL;
    accept = NULL;
    accept_encoding = NULL;
    accept_language = NULL;
    cookie = NULL;
    content = NULL;
    content_length = 0;

    check_state = CHECK_STATE_REQUESTLINE;
    start_line = 0;
    checked_index = 0;
    read_index = 0;
    
    write_index = 0;
    m_bytes_have_send = 0;

    unmap();
    
    memset(read_buffer, '\0', HTTP_READ_BUFFER_SIZE);
    memset(write_buffer, '\0', HTTP_WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILE_NAME_SIZE);
    memset(m_iv, 0, sizeof(m_iv));
}


/**
 * @brief 完全客户端请求内容
 * 
 * @return HTTP_CODE 
 */
HTTP_CODE http_req_t::do_request() {
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, this->url, FILE_NAME_SIZE - len - 1);
    if (stat(m_real_file, &m_file_stat) < 0) {
        return NO_RESOURCE;
    }

    if (!(m_file_stat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST;
    }

    if (S_ISDIR(m_file_stat.st_mode)) {
        return BAD_REQUEST;
    }

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (m_file_address == NULL) {
        return BAD_REQUEST;
    }

    return FILE_REQUEST;
}



/**
 * @brief 根据http_parse的解析结果，决定返回给客户端的内容
 * 
 * @param ret 
 * @return true 
 * @return false 
 */
bool http_req_t::http_write_buffer(HTTP_CODE ret) {
    switch (ret) {
        case INTERNAL_ERROR: {
            http_rep_status_line(500, error_500_title);
            http_rep_headers(strlen(error_500_form));
            
            return http_rep_content(error_500_form);
        } break;

        case BAD_REQUEST: {
            http_rep_status_line(400, error_400_title);
            http_rep_headers(strlen(error_400_form));
            return http_rep_content(error_400_form);
        } break;

        case FORBIDDEN_REQUEST: {
            http_rep_status_line(403, error_403_title);
            http_rep_headers(strlen(error_403_form));
            return http_rep_content(error_403_form);

        } break;

        case NO_RESOURCE: {
            http_rep_status_line(404, error_404_title);
            http_rep_headers(strlen(error_404_form));
            return http_rep_content(error_404_form);
        } break;

        case FILE_REQUEST: {
            http_rep_status_line(200, ok_200_title);
            //将http响应头和文件内容放入集中写的目标内存中
            if (m_file_stat.st_size != 0) {
                http_rep_headers(m_file_stat.st_size);
                m_iv[0].iov_base = write_buffer;
                m_iv[0].iov_len = write_index;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                return true;
            } else {
                const char *ok_string = "<html><body></body></html>";
                http_rep_headers((strlen(ok_string)));
                return http_rep_content(ok_string);
            }
        } break;

        default:
            return false;
    }

    m_iv[0].iov_base = write_buffer;
    m_iv[0].iov_len = write_index;
    m_iv_count = 1;

    return true;
}

//添加响应行
bool http_req_t::http_rep_status_line(int status, const char *title) {
    return add_response("HTTP/1.1 %d %s\r\n", status, title);
}

//添加响应头
bool http_req_t::http_rep_headers(int content_len) {
    return http_rep_content_length(content_len) |
    http_rep_is_keepalive() |
    http_rep_blank_line();
}


bool http_req_t::http_rep_content_length(int content_len) {
    return add_response("Content-Length: %d\r\n", content_len);
}

bool http_req_t::http_rep_is_keepalive() {
    return add_response("Connection: %s\r\n", this->connect ? "keep-alive" : "close");
}

bool http_req_t::http_rep_blank_line() {
    return add_response("\r\n");
}

bool http_req_t::http_rep_content(const char *content) {
    return add_response("%s", content);
}


/**
 * @brief 往http写缓冲区写入待发送的数据
 * 
 * @param format 
 * @param ... 
 * @return true 
 * @return false 
 */
bool http_req_t::add_response(const char *format, ...) {
    if (write_index >= HTTP_WRITE_BUFFER_SIZE) {
        return false;
    }

    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(write_buffer + write_index, 
                        HTTP_WRITE_BUFFER_SIZE - 1 - write_index,
                        format,
                        arg_list);

    if (len >= HTTP_WRITE_BUFFER_SIZE - 1 - write_index) {
        return false;
    }

    write_index += len;
    va_end(arg_list);

    return true;
}




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
        //解析方法
    if (strcasecmp(method, "GET") == 0) {
        this->method = GET;
    } else if (strcasecmp(method, "PUT") == 0) {
        this->method = PUT;
    } else if (strcasecmp(method, "PUSH") == 0) {
        this->method = PUSH;
    } else if (strcasecmp(method, "DELETE") == 0) {
        this->method = DELETE;
    } else if (strcasecmp(method, "HEAD") == 0) {
        this->method = HEAD;
    } else if (strcasecmp(method, "POST") == 0) {
        this->method = POST;
    } else {
        return BAD_REQUEST;
    }


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
        //如果http请求有消息体，则还需要读取content_length字节的消息体
        //并将状态转移到CHECK_STATE_CONTENT;
        if (this->content_length != 0) {
            this->check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
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
 * @brief 解析请求体的内容，只简单的获取即可
 * 
 * @param temp 
 * @return HTTP_CODE 
 */
HTTP_CODE http_req_t::parse_content(char *temp) {
    if (temp == NULL) {
        return BAD_REQUEST;
    }

    if (this->content == NULL) {
        this->content = temp;
    }


    if (read_index >= (content_length + checked_index)) {

        const_cast<char*>(this->content)[this->content_length] = '\0';
        return GET_REQUEST;
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
HTTP_CODE http_req_t::http_parse() {

        LINE_STATUS line_status = LINE_OK;
        HTTP_CODE retcode = NO_REQUEST;

        //主状态机，用于从buffer中取出所有完整的行

        while ((check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) 
                || (line_status = parse_line()) == LINE_OK) {
            
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
                if (retcode == BAD_REQUEST) {
                    return retcode;
                } else if (retcode == GET_REQUEST) {
                    return do_request();
                }
            } break;

            case CHECK_STATE_CONTENT: {
                retcode = parse_content(temp);
                if (retcode == BAD_REQUEST) {
                    return retcode;
                } else if (retcode == GET_REQUEST) {
                    return do_request();
                }

                line_status = LINE_OPEN;
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
ssize_t http_req_t::http_read(int connfd) {
    if (read_index >= HTTP_READ_BUFFER_SIZE) {
        return -1;
    }

    int bytes_read = 0;
    bytes_read = recv(connfd, read_buffer + read_index, HTTP_READ_BUFFER_SIZE - read_index, 0);

    if (bytes_read <= 0) {
        return bytes_read;
    }

    printf("read %d bytes from %d\n", bytes_read, connfd);
    read_index += bytes_read;
    
    return bytes_read;
}

}




#endif