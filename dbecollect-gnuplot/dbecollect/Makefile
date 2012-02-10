SVNVERSION := 0.$(shell svn info -r HEAD . | grep "^Revision:" | tr -d -c [0-9])

UTIL= collect


HOSTCC = gcc
CROSSCC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall
#CFLAGS += -DDEBUG
#CFLAGS += -ggdb

LDFLAGS = -static 
#LDFLAGS += -s

KATCP = ../../katcp
PLOT = ../gnuplot_i-2.10/src
SUB = 
INC = -I$(KATCP) -I$(PLOT)
LIB = ../gnuplot_i-2.10/gnuplot_i.o -L$(KATCP) -lm


RM = rm -f

OBJ = $(patsubst %.c,%.o,$(SRC))

all: $(EXE) $(UTIL)

$(EXE): $(OBJ)
	$(CROSSCC) $(LDFLAGS) -o $@ $(OBJ) $(LIB)

$(UTIL): $(UTIL).c
	$(HOSTCC) $(CFLAGS) $(INC) -o $@ $^ $(LIB)

sub: 
	@for d in $(SUB); do if ! $(MAKE) -C $$d CFLAGS="$(CFLAGS)" ; then exit; fi; done

%.o: %.c $(wildcard *.h)
	$(CROSSCC) $(CFLAGS) -DSVNVERSION=\"$(SVNVERSION)\" -c $< $(INC)

clean:
	@for d in $(SUB); do if ! $(MAKE) -C $$d clean ; then exit; fi; done
	$(RM) $(OBJ) core $(UTIL)
