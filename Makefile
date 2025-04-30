# Компилятор
CC = gcc

# Флаги компиляции
CFLAGS = -Wall -Wextra -std=gnu99 -Iincludes

# Имя исполняемого файла
TARGET = tdo

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))
HEADERS = $(wildcard includes/*.h)
BUILD_DIR = build

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR):
	mkdir -p $@

# build
$(BUILD_DIR)/$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(OBJS) -o $@

# src/*.c -> build/*.o
$(BUILD_DIR)/%.o: src/%.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
