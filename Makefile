# Компилятор
CC = gcc

# Флаги компиляции
CFLAGS = -Wall -Wextra -std=gnu99

# Имя исполняемого файла
TARGET = tdo

# Исходные файлы
SRCS = main.c file_utils.c term.c

# Объектные файлы
OBJS = $(SRCS:.c=.o)

# Правило по умолчанию
all: $(TARGET)

# Сборка программы
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Правило для компиляции .c файлов в .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Очистка
clean:
	rm -f $(OBJS) $(TARGET)

