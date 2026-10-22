# Makefile for EECE 446 - Program 4
# Builds the registry executable by default
# Cleans up all generated files

CC = gcc
CFLAGS = -Wall

# Default target: build registry
all: registry

registry: registry.c
	$(CC) $(CFLAGS) -o registry registry.c

clean:
	rm -f registry

