CC = gcc
CFLAGS = -Wall -std=c99 -D_GNU_SOURCE
LIBS = -lpthread -lrt

all: burger_system

burger_system: burger_system.o
	$(CC) -o burger_system burger_system.o $(LIBS)

burger_system.o: burger_system.c
	$(CC) $(CFLAGS) -c burger_system.c

.PHONY: clean run help
clean:
	rm -f burger_system control_panel *.o

run: burger_system
	./burger_system -n 4

help: burger_system
	./burger_system --help

