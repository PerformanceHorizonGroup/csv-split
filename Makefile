CC:=$(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')
LINK=-lcsv -lpthread
DEBUG?=-g -ggdb
OPTIMIZATION?=-O3
CFLAGS=$(DEBUG) $(OPTIMIZATION) $(LINK)
INSTALL_PATH?=/usr/local
BIN=csv-split
DEPS=csv-split.h csv-buf.h queue.h
MANPREFIX?=/usr/share/man/man1
MANPAGE=csv-split.1
MANCMP=csv-split.1.gz

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
	rm -f *.o *.gz $(BIN)

install: all
	gzip -c $(MANPAGE) > $(MANPAGE).gz && cp -pf $(MANPAGE).gz $(MANPREFIX)
	cp -pf $(BIN) $(INSTALL_PATH)/bin

dep: 
	$(CC) -MM *.c

