CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = server
BUILD_DIR = build

SRCS = main.c Server.c HTTPRequest.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean run dirs

all: dirs $(BUILD_DIR)/$(TARGET)

dirs:
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

run: clean all
	./$(BUILD_DIR)/$(TARGET)

