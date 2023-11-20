#ifndef HTTP_CONN_HPP
#define HTTP_CONN_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/epoll.h>

#include "http_parse.hpp"


namespace http_conn_space {

    int setnonblocking(int fd) {
        int old_opt = fcntl(fd, F_GETFL);
        int new_opt = old_opt | O_NONBLOCK;
        fcntl(fd, F_SETFL, new_opt);

        return old_opt;
    }

    void addfd(int epollfd, int fd, bool one_shot) {
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLRDHUP;

        if (one_shot) {
            event.events |= EPOLLONESHOT;
        }

        epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
        setnonblocking(fd);
    }

    void removefd(int epollfd, int fd) {
        epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
    }

    void modfd(int epollfd, int fd, int ev) {
        epoll_event event;
        event.data.fd = fd;
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
        epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);

    }



 

    using namespace http_req_space;

  
    
    class http_conn {

    public:
        http_conn(){};
        ~http_conn(){};

        void init(int sockfd, const sockaddr_in& addr);
        void close_conn(bool real_close = true);
        void process(); //线程池入口，进行逻辑处理
        bool read();    //非阻塞读
        bool write();   //非阻塞写
    
    public:
        static int m_epollfd;   //该连接所属epoll内核事件表
        static int m_user_count;    //统计用户数量

    private:
        int m_sockfd{};   //连接的socket
        sockaddr_in m_address{};  //连接的socket地址

        http_req_t m_http_req{};    //维护当前连接的状态



    };

    /**
     * @brief 关闭连接
     * 
     * @param real_close 
     */
    void http_conn::close_conn(bool real_close) {
        if (real_close && (m_sockfd != -1)) {
            removefd(m_epollfd, m_sockfd);
            m_sockfd = -1;
            m_user_count--;
        }
    }

    /**
     * @brief 初始化连接
     * 
     */
    void http_conn::init(int sockfd, const sockaddr_in& addr) {
        m_sockfd = sockfd;
        m_address = addr;
        
        //强制重用ip地址和端口
        int reuse = 1;
        setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        addfd(m_epollfd, sockfd, true);

        m_user_count++;

        //重置http连接状态
        m_http_req.http_reset();
    }

    /**
     * @brief 由线程池中的工作线程调用，这是处理http请求的入口函数
     * 
     */
    void http_conn::process() {
        HTTP_CODE read_ret = this->m_http_req.http_parse();
        if (read_ret == NO_REQUEST) {
            modfd(m_epollfd, m_sockfd, EPOLLIN);
            return;
        }

        bool write_ret = this->m_http_req.http_write(read_ret);
        if (!write_ret) {
            close_conn();
        }

        //http写缓冲区已就绪，注册写事件
        modfd(m_epollfd, m_sockfd, EPOLLOUT);
    }

    /**
     * @brief 读取客户端数据，直到无数据可读或对方关闭连接
     * 
     */
    bool http_conn::read() {
        return this->m_http_req.http_read(this->m_sockfd);
    }


    /**
     * @brief 往客户端写数据
     * 
     * @return true 
     * @return false 
     */
    bool http_conn::write() {
        bool flag = this->m_http_req.http_write(m_sockfd);
        if (flag) {
            if (errno == EAGAIN) {  //内容未发送完毕,再次注册写事件
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
            } else {    //内容发送完毕，连接保持，注册读事件
                modfd(m_epollfd, m_sockfd, EPOLLIN);
            }
        } 

        return flag;
    }
 



}






#endif