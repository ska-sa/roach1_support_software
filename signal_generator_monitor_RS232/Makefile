CC = gcc
CFLAGS = -Wall -ggdb

all: sg_monitor 

roach_server: sg_monitor.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f sg_monitor
