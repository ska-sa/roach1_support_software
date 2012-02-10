CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall
#CFLAGS += -DDEBUG -ggdb
INC = -I..
LIB = -L.. -lkatcp
RM = rm -f
BUILD = unknown-0.1

EXE = dispatch-test line netc echo-test

all: $(EXE)

line: line.c
	$(CC) $(CFLAGS) $(INC) -DUNIT_TEST_LINE -o $@ $^

netc: netc.c
	$(CC) $(CFLAGS) $(INC) -DUNIT_TEST_NETC -o $@ $^

dispatch-test: dispatch-test.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^ $(LIB)

echo-test: echo-test.c
	$(CC) -DBUILD=\"$(BUILD)\" $(CFLAGS) $(INC) -o $@ $^ $(LIB)

clean: 
	$(RM) $(EXE) *.template *.output

test: all
	./test-line.sh
