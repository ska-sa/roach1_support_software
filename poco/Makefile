SVNVERSION := 0.$(shell svn info -r HEAD . | grep "^Revision:" | tr -d -c [0-9])

NAME= tcpborphserver2
EXE = $(NAME)
UTIL= collect

SRC = main.c core.c tgtap.c raw.c poco.c misc.c pce.c upload.c echo.c capture.c udp.c gain.c dram.c word.c fixer.c snapshot.c mcount.c sensors.c ntp.c
# fixer.c 
FILES = Makefile $(SRC) $(wildcard *.h)

HOSTCC = gcc
CROSSCC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall
#CFLAGS += -DDEBUG
CFLAGS += -ggdb

#LDFLAGS = -static 
#LDFLAGS += -s

KATCP = ../lib/katcp
SUB = 
INC = -I$(KATCP)
LIB = -L$(KATCP) -lkatcp

#SUB = katcp
#INC = $(foreach d,$(SUB),-I$(d))
#LIB = $(foreach d,$(SUB),$(if $(wildcard $(d)/lib$(d).a),-L$(d) -l$(d)))

RM = rm -f

OBJ = $(patsubst %.c,%.o,$(SRC))

all: $(EXE) $(UTIL)

$(EXE): $(OBJ)
	$(CROSSCC) $(LDFLAGS) -o $@ $(OBJ) $(LIB)

$(UTIL): $(UTIL).c
	$(HOSTCC) -o $@ $^

sub: 
	@for d in $(SUB); do if ! $(MAKE) -C $$d CFLAGS="$(CFLAGS)" ; then exit; fi; done

%.o: %.c $(wildcard *.h)
	$(CROSSCC) $(CFLAGS) -DSVNVERSION=\"$(SVNVERSION)\" -c $< $(INC)

clean:
	@for d in $(SUB); do if ! $(MAKE) -C $$d clean ; then exit; fi; done
	$(RM) $(OBJ) core $(EXE) $(UTIL)
