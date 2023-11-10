# 定义编译器和编译选项
CC = g++
CFLAGS = -Wall -g -Iinc

# 定义目标文件和依赖文件
TARGET = main
OBJS = main.o 

# 定义生成目标文件的规则
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# 定义生成依赖文件的规则
src/%.o: %.c 
	$(CC) $(CFLAGS) -c $<

# 定义清理文件的规则
clean:
	rm -f $(TARGET) $(OBJS)
