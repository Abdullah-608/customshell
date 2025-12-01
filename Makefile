# Makefile for Custom Shell

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = 
TARGET = shell.exe
SOURCES = main.c shell.c parser.c builtins.c vfs.c interpreter.c process.c utils.c file_helpers.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = shell.h parser.h builtins.h vfs.h interpreter.h process.h utils.h file_helpers.h

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET) vfs.dat

# Rebuild everything
rebuild: clean all

# Run the shell
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean rebuild run

