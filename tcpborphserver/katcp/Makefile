prefix=/usr/local

CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall
CFLAGS += -DKATCP_USE_FLOATS
#CFLAGS += -DDEBUG
CFLAGS += -ggdb
AR = ar
RM = rm -f
INSTALL = install
INC = -I.

LIB = libkatcp.a

SUB = examples utils tests
SRC = line.c netc.c dispatch.c loop.c log.c time.c shared.c misc.c server.c client.c ts.c nonsense.c
HDR = katcp.h katcl.h katsensor.h katpriv.h
OBJ = $(patsubst %.c,%.o,$(SRC))

all: $(TESTS) $(LIB) $(UTILS) sub

$(LIB): $(OBJ)
	$(AR) rcs $(LIB) $(OBJ)

sub: 
	@for d in $(SUB); do if ! $(MAKE) -C $$d CFLAGS="$(CFLAGS)" ; then exit; fi; done

%.o: %.c *.h 
	$(CC) $(CFLAGS) -c $< -o $@ $(INC)

clean:
	$(RM) $(TESTS) $(LIB) $(UTILS) *.o core
	@for d in $(SUB); do if ! $(MAKE) -C $$d $@; then exit; fi; done

install: all
	$(INSTALL) -d $(prefix)/include $(prefix)/lib
	$(INSTALL) $(LIB) $(prefix)/lib
	$(INSTALL) $(HDR) $(prefix)/include

