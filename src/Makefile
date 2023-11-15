# 定义编译器和编译选项
CC = g++
CFLAGS = -Wall -g

# 定义子目录列表
SUBDIRS = test_accept test_listen test_oobrecv test_oobsend \
set_recv_buffer set_send_buffer test_getaddrinfo test_uid	\
http_parse test_select test_epoll test_epolloneshot \
chat_client chat_server handle_tcp_udp


# 定义清理规则
.PHONY: clean

# 递归调用子目录中的makefile
all:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
		done

# 清理所有生成的文件
clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
		done
