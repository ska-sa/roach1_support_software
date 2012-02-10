CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall
CFLAGS += -DDEBUG -ggdb
INC = -I..
LIB = -L.. -lkatcp
RM = rm -f

EXE = katcpcmd

$(EXE): cmd.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^ $(LIB)

clean: 
	$(RM) $(EXE)
