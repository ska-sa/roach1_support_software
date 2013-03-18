###############################################################################
# Add the list of modes to enable to the MODES variable, use space as separator
# available modes are: raw poco holo

# select the modes to include
# MODES = poco raw holo
# MODES = raw holo 
# MODES = raw poco
MODES = raw 

# compile option, to control debugging output and debugger support
CFLAGS = -Wall
CFLAGS += -O2
#CFLAGS += -DDEBUG
#CFLAGS += -DDEBUG=\"2\"
#CFLAGS += -DHACK
CFLAGS += -ggdb
#LDFLAGS = -static -s 
LDFLAGS = -s 
SVN = svn
GREP = grep
TR = tr

# where to find the KATCP library (change this in case katcp isn't included locally)
KATCP = katcp

# end of user serviceable parts
###############################################################################

CC = $(CROSS_COMPILE)gcc
VERFILE = .version

# SVNVERSION := 0.$(shell svn info -r HEAD . | grep "^Revision:" | tr -d -c [0-9])
CFLAGS += -DVERSION=\"2.$(shell if test -f $(VERFILE) ; then cat $(VERFILE) ; else date +0a%s; fi)\"

SERVER = tcpborphserver2

SUB = collect katcp core

ifeq (raw,$(findstring raw,$(MODES)))
SUB += raw
CFLAGS += -DHAVE_MODE_RAW 
endif

ifeq (poco,$(findstring poco,$(MODES)))
SUB += poco
CFLAGS += -DHAVE_MODE_POCO 
endif

ifeq (holo,$(findstring holo,$(MODES)))
SUB += holo 
CFLAGS += -DHAVE_MODE_HOLO 
endif

MODES += core

RM = rm -f

all: $(SERVER)
#all: all-dir $(SERVER)

clean: clean-dir
	$(RM) $(SERVER)

reallyclean: clean
	$(RM) $(VERFILE)

install: install-dir $(SERVER) 

# warning: below rewrites KATCP for subdirectory
all-dir clean-dir install-dir: 
	@for d in $(SUB); do if ! $(MAKE) -C $$d KATCP=../$(KATCP) CFLAGS="$(CFLAGS)" $(subst -dir,,$@) ; then exit; fi; done

$(SERVER): $(SERVER).c all-dir
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -I$(KATCP) -Iinclude $(foreach f,$(MODES),$(f)/$(f).a) -L$(KATCP) -lkatcp
