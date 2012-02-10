CROSS=ppc_4xx

CC=$(CROSS)-gcc
OBJCOPY=$(CROSS)-objcopy
OBJDUMP=$(CROSS)-objdump
BIN2OCD=bin2ocd
CFLAGS=-Wall 
CFLAGS+=-Os
LD=$(CROSS)-ld
RM=rm -f
CAT=cat

#PROJECT=interactive
#CFLAGS+=-DRINIT_INTERACTIVE

PROJECT=program
CFLAGS+=-DRINIT_PROGRAM

SRC=crt0.S serial.S boot.S intt.S flush.S lib.c xmodem.c flash.c $(PROJECT).c
OBJ=crt0.o serial.o boot.o intt.o flush.o lib.o xmodem.o flash.o $(PROJECT).o
PREPEND=macros/ram-setup.mac

EXE=$(PROJECT).elf
BIN=$(PROJECT).bin
MAC=$(PROJECT).mac
DUMP=$(PROJECT)-dis.S
MAP=$(PROJECT).map
LDS=config.ls

all: $(EXE) $(BIN) $(DUMP) $(MAC)

%.o: %.c *.h
	$(CC) $(CFLAGS) -fomit-frame-pointer -c $< -o $@

%.o: %.S *.h
	$(CC) $(CFLAGS) -c $< -o $@

$(EXE): $(OBJ) $(LDS)
	$(LD) -M -o $(EXE) $(LDS) $(OBJ) > $(MAP)

$(DUMP): $(EXE)
	$(OBJDUMP) -d $(EXE) > $(DUMP)

$(BIN): $(EXE)
	$(OBJCOPY) -O binary $< $@

$(MAC): $(BIN)
	$(CAT) $(PREPEND) > $@
	$(BIN2OCD) -b 0x71000000 -p 0x71001000 $< >> $@

clean:
	$(RM) $(OBJ) $(EXE) $(BIN) $(MAC) $(DUMP) $(MAP)
