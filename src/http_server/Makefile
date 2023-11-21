# 定义目标文件和依赖文件
CC = g++
TARGET = main
OBJS = main.o
CFLAGS = -Wall -g -pthread

# 定义生成目标文件的规则
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# 定义生成依赖文件的规则
src/%.o: %.c %.cpp
	$(CC) $(CFLAGS) -c $<

# 定义清理文件的规则
clean:
	rm -f $(TARGET) $(OBJS)
