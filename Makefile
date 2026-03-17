CC = gcc
CFLAGS = -O3 -Wall -Wextra -Werror -std=c11

SOURCES = lisp.c
TARGET = lisp

all: clean build

build:
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(SOURCES)

.PHONY: all clean build
