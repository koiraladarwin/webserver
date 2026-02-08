# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iincludes

# Project folders
SRC_DIR = src
BUILD_DIR = build
TARGET = server

# Source and object files
SRCS = $(SRC_DIR)/test.c \
       $(SRC_DIR)/Server.c \
       $(SRC_DIR)/HTTPRequest.c \
       $(SRC_DIR)/HTTPResponse.c \
       $(SRC_DIR)/HTTPServer.c \
       $(SRC_DIR)/Helper.c

OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Phony targets
.PHONY: all clean run dirs

# Default target
all: dirs $(BUILD_DIR)/$(TARGET)

# Create build directory
dirs:
	mkdir -p $(BUILD_DIR)

# Link the target
$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# Compile each source file
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Build and run
run: clean all
	sudo ./$(BUILD_DIR)/$(TARGET)
