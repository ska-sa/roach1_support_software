CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall
#CFLAGS += -DDEBUG
#CFLAGS += -DKATCP
CFLAGS += -ggdb
AR = ar
RM = rm -f

INC = -I../include -I$(KATCP)
LIB = holo.a
SRC = capture.c udp.c ntp.c holo.c snapshot.c bram.c adc.c katadc.c

OBJ = $(patsubst %.c,%.o,$(SRC))

all: $(LIB) 

$(LIB): $(OBJ)
	$(AR) rcs $(LIB) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INC)

clean:
	$(RM) $(LIB) *.o core

