
CC = gcc
CFLAGS = -Wall -std=c99 -D_GNU_SOURCE
LIBS = -lpthread -lrt -lncurses

all: burger_system control_panel

burger_system: burger_system.o
	$(CC) -o burger_system burger_system.o $(LIBS)

control_panel: control_panel.o  
	$(CC) -o control_panel control_panel.o $(LIBS)

burger_system.o: burger_system.c
	$(CC) $(CFLAGS) -c burger_system.c

control_panel.o: control_panel.c
	$(CC) $(CFLAGS) -c control_panel.c

.PHONY: clean run help
clean:
	rm -f burger_system control_panel *.o

run: burger_system
	./burger_system -n 4

help: burger_system
	./burger_system --help