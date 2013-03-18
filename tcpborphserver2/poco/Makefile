CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall
#CFLAGS += -DDEBUG
CFLAGS += -ggdb
AR = ar
RM = rm -f

INC = -I../include -I$(KATCP)
LIB = poco.a
SRC = capture.c dram.c fixer.c gain.c mcount.c ntp.c poco.c sensors.c snapshot.c udp.c

OBJ = $(patsubst %.c,%.o,$(SRC))

all: $(LIB) 

$(LIB): $(OBJ)
	$(AR) rcs $(LIB) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INC)

clean:
	$(RM) $(LIB) *.o core

