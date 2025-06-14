# Compiler settings
CC      := gcc
CFLAGS  := -g -Wall -Wextra -Wpedantic
LDFLAGS := 

# Project name
TARGET := huf

# Source files
SRC_DIR := .
HUF_DIR := huff
TREE_DIR := $(HUF_DIR)/tree

SOURCES := \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/archiver.c \
    $(HUF_DIR)/node.c \
    $(TREE_DIR)/builder.c \
    $(TREE_DIR)/codes.c \
    $(SRC_DIR)/buffio.c \
    $(SRC_DIR)/progbar.c \
    $(SRC_DIR)/queue.c \
    $(SRC_DIR)/filetools.c

# Object files
OBJECTS := $(SOURCES:.c=.o)

# Default target
all: $(TARGET) clean

# Link object files to create executable
$(TARGET): $(OBJECTS)
	@echo "Linking $@..."
	@$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
	@echo "Build successful!"

# Compile source files to object files
%.o: %.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning..."
	@rm -f $(OBJECTS)
	@echo "Clean complete."

# Phony targets (not files)
.PHONY: all clean