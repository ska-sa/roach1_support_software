SRC = main.c client.c cmds.c leaf.c msg.c relay.c xlookup.c xtest.c inter.c
SUB = loop katcp
TESTS = test-msg


MAJOR = 0
MINOR = 1
EXE = multiserver

CC = gcc
CFLAGS = -Wall
CFLAGS += -DDEBUG -ggdb
RM = rm -f

OBJ = $(patsubst %.c,%.o,$(SRC))
INC = $(foreach d,$(SUB),-I$(d))

############################################

all: $(EXE)

tests: $(TESTS)

$(EXE): sub $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(foreach d,$(SUB),$(if $(wildcard $(d)/lib$(d).a),-L$(d) -l$(d)))

sub: 
	@for d in $(SUB); do if ! $(MAKE) -C $$d CFLAGS="$(CFLAGS)" ; then exit; fi; done

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -DMAJOR=$(MAJOR) -DMINOR=$(MINOR) -c $< $(INC)

test-msg: msg.c
	$(CC) $(LDFLAGS) $(CFLAGS) $(INC) -DUNIT_TEST_MSG -o $@ $^ $(foreach d,$(SUB),$(if $(wildcard $(d)/lib$(d).a),-L$(d) -l$(d)))

clean:
	$(RM) $(OBJ) core $(EXE) $(TESTS)
