CC=$(CROSS_COMPILE)gcc
CFLAGS_DEFAULT=-Wall -Werror -lpthread
CFLAGS_OPT?=
INSTALL_PREFIX_BIN?=/usr/bin

all: edac-mon

edac-mon: edac-mon.c
	$(CC) $(CFLAGS_DEFAULT) -o edac-mon edac-mon.c $(CFLAGS_OPT)

install:
	cp edac-mon $(INSTALL_PREFIX_BIN)/

clean:
	rm -f edac-mon *.o
