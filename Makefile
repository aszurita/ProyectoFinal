CC = gcc
CFLAGS = -Wall -std=c99 -D_GNU_SOURCE -c
LIBS = -lpthread -lrt

# Meta principal: compilar sistema y panel de control
all: burger_system control_panel

burger_system: burger_system.o
	$(CC) -o burger_system burger_system.o $(LIBS)

control_panel: control_panel.o
	$(CC) -o control_panel control_panel.o $(LIBS) -lncurses

burger_system.o: burger_system.c
	$(CC) $(CFLAGS) burger_system.c

control_panel.o: control_panel.c
	$(CC) $(CFLAGS) control_panel.c

.PHONY: clean run
clean:
	rm -f burger_system control_panel *.o

run: burger_system
	./burger_system -n 4