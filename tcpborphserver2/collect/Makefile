CC = gcc
CFLAGS = -Wall
#CFLAGS += -DDEBUG
CFLAGS += -ggdb
RM = rm -f

INC = -I../include
EXE = kdpcollect
SRC = collect.c

OBJ = $(patsubst %.c,%.o,$(SRC))

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LIB)

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c $< $(INC)

clean:
	$(RM) $(OBJ) core $(EXE)
