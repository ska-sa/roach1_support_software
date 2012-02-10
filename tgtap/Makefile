CC = $(CROSS_COMPILE)gcc

CFLAGS = -Wall
#CFLAGS += -DDEBUG
#CFLAGS += $(shell test `whoami` = marc && echo "-DDEBUG -ggdb")
AR = ar
RM = rm -f
INSTALL = install
LDFLAGS = 

GETAP = tgtap

LIB = 
SRC = tapper.c tgtap.c
HDR = tapper.h 
OBJ = $(patsubst %.c,%.o,$(SRC))

$(GETAP): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIB)

%.o: %.c *.h 
	$(CC) $(CFLAGS) -c $< -o $@ $(INC)

clean:
	$(RM) $(GETAP) *.o

install: all
	$(INSTALL) $(GETAP) $(prefix)/bin

