CC ?= gcc
CFLAGS += -Wall -Werror
LDFLAGS += -lpthread
INSTALL_PREFIX_BIN ?= /usr/bin

all: edac-mon

edac-mon: edac-mon.c
	$(CC) $(CFLAGS) -o edac-mon edac-mon.c $(LDFLAGS)

install:
	cp edac-mon $(INSTALL_PREFIX_BIN)/

clean:
	rm -f edac-mon *.o
