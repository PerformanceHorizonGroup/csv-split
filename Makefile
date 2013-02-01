CC:=$(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')
LINK=-lcsv -lpthread
DEBUG?=-g -ggdb
OPTIMIZATION?=-O3
CFLAGS=$(DEBUG) $(OPTIMIZATION) $(LINK)
INSTALL_PATH?=/usr/local
BIN=csv-split
DEPS=csv-split.h csv-buf.h queue.h

.PHONY: debug

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

csv-split: queue.o csv-buf.o csv-split.o
	$(CC) -o $(BIN) csv-buf.o queue.o csv-split.o $(CFLAGS)

debug:
	$(MAKE) OPTIMIZATION=""

all:
	$(MAKE) DEBUG=""

clean:
	rm -f *.o $(BIN)

install: all
	cp -pf $(BIN) $(INSTALL_PATH)/bin

dep: 
	$(CC) -MM *.c

