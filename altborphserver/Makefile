CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall
CFLAGS += -DDEBUG

KATCP = ../lib/katcp

INC = -I$(KATCP)
LIB = -L$(KATCP) -lkatcp
RM = rm -f

EXE = altborphserver

all: $(EXE)

altborphserver: altborphserver.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^ $(LIB)

clean: 
	$(RM) $(EXE)
