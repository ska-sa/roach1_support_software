CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall
#CFLAGS += -DDEBUG
CFLAGS += -ggdb
AR = ar
RM = rm -f

INC = -I../include -I$(KATCP)
LIB = raw.a
SRC = announce.c raw.c tgtap.c upload.c word.c echo.c

OBJ = $(patsubst %.c,%.o,$(SRC))

all: $(LIB) 

$(LIB): $(OBJ)
	$(AR) rcs $(LIB) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INC)

clean:
	$(RM) $(LIB) *.o core

