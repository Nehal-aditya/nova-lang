# NOVA Parser Makefile
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
LEXER_INC_DIR = ../lexer/include
LEXER_LIB_DIR = ../lexer/build

# Source files
SRCS = $(SRC_DIR)/parser.c $(SRC_DIR)/ast.c
OBJS = $(BUILD_DIR)/parser.o $(BUILD_DIR)/ast.o

# FFI source
FFI_SRC = $(SRC_DIR)/parser_ffi.c
FFI_OBJ = $(BUILD_DIR)/parser_ffi.o
FFI_LIB = $(LIB_DIR)/libnova_parser_ffi.so

# Include paths
INCLUDES = -I$(INC_DIR) -I$(LEXER_INC_DIR)

# Library dependencies
LEXER_LIB = $(LEXER_LIB_DIR)/libnova_lexer.a

.PHONY: all clean test ffi

all: $(LIB_DIR)/libnova_parser.a

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIB_DIR)/libnova_parser.a: $(OBJS) | $(LIB_DIR)
	$(AR) $(ARFLAGS) $@ $(OBJS)

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

# Build lexer first if needed
$(LEXER_LIB):
	$(MAKE) -C ../lexer all

# Shared library for FFI
ffi: $(LEXER_LIB) $(FFI_LIB)

$(FFI_LIB): $(FFI_OBJ) $(LEXER_LIB) | $(BUILD_DIR)
	$(CC) -shared -o $@ $(FFI_OBJ) -L$(LEXER_LIB_DIR) -lnova_lexer -L$(LIB_DIR) -lnova_parser

$(BUILD_DIR)/parser_ffi.o: $(FFI_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(LIB_DIR)/libnova_parser.a $(FFI_LIB)
