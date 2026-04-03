# NOVA Lexer Makefile
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -fPIC
AR = ar
ARFLAGS = rcs

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
LIB_DIR = ../build

# Source files
SRCS = $(SRC_DIR)/lexer.c $(SRC_DIR)/token.c
OBJS = $(BUILD_DIR)/lexer.o $(BUILD_DIR)/token.o

# Test files
TEST_SRCS = $(TEST_DIR)/test_lexer.c
TEST_OBJS = $(BUILD_DIR)/test_lexer.o
TEST_BIN = $(BUILD_DIR)/test_lexer

# Library name
LIB_NAME = libnova_lexer.a
FFI_LIB_NAME = libnova_lexer.so

# Include path
INCLUDES = -I$(INC_DIR)

.PHONY: all clean test ffi

all: $(LIB_DIR)/$(LIB_NAME)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIB_DIR)/$(LIB_NAME): $(OBJS) | $(LIB_DIR)
	$(AR) $(ARFLAGS) $@ $(OBJS)

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

# Shared library for FFI
ffi: $(BUILD_DIR)/$(FFI_LIB_NAME)

$(BUILD_DIR)/$(FFI_LIB_NAME): $(OBJS) | $(BUILD_DIR)
	$(CC) -shared -o $@ $(OBJS)

# Test executable
test: $(TEST_BIN)
	./$(TEST_BIN)

$(BUILD_DIR)/test_lexer.o: $(TEST_DIR)/test_lexer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_BIN): $(BUILD_DIR)/test_lexer.o $(LIB_DIR)/$(LIB_NAME)
	$(CC) $(CFLAGS) -L$(LIB_DIR) -o $@ $(BUILD_DIR)/test_lexer.o -lnova_lexer

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(LIB_DIR)/$(LIB_NAME) $(LIB_DIR)/$(FFI_LIB_NAME)
