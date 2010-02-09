C99 ?= c99
CFLAGS := -pedantic -Wall -Wextra $(CFLAGS)

.PHONY: all clean

all: libipc.a

clean:
	rm -f *.[ao]

libipc.a: avl.o mpool.o ipc.o io_file.o
	ar -c -r $@ $?

avl.o: avl.h
mpool.o: mpool.h
ipc.o: ipc.h mpool.h io.h
io_file.o: io_file.h io.h

%.o: %.c
	$(C99) $(CFLAGS) -c -o $@ $<
