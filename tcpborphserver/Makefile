SVNVERSION := 0.$(shell svn info . | grep "^Revision:" | tr -d -c [0-9])
SRC = tcpborphserver.c
SUB = katcp

CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall
#CFLAGS += -DDEBUG
#CFLAGS += -ggdb

LDFLAGS = -static -s

KATCP = ../lib/katcp

INC = -I$(KATCP)
LIB = -L$(KATCP) -lkatcp
#INC = -I/tmp/include
#LIB = -L/tmp/lib -lkatcp
RM = rm -f

OBJ = $(patsubst %.c,%.o,$(SRC))
INC = $(foreach d,$(SUB),-I$(d))

EXE = tcpborphserver

all: $(EXE)

$(EXE): sub $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(foreach d,$(SUB),$(if $(wildcard $(d)/lib$(d).a),-L$(d) -l$(d)))

sub: 
	@for d in $(SUB); do if ! $(MAKE) -C $$d CFLAGS="$(CFLAGS)" ; then exit; fi; done

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -DSVNVERSION=\"$(SVNVERSION)\" -c $< $(INC)

clean:
	@for d in $(SUB); do if ! $(MAKE) -C $$d clean ; then exit; fi; done
	$(RM) $(OBJ) core $(EXE) $(TESTS)
