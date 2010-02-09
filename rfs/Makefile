C99 ?= c99
IPCDIR = ../ipc
CFLAGS := -pedantic -Wall -Wextra -I $(IPCDIR) $(CFLAGS)
LDFLAGS := -L $(IPCDIR) $(LDFLAGS)
LIBS += -lipc
XSLTFLAGS += --path $(IPCDIR)

bin = rfs rfsd

.PHONY: all
all: $(bin)

clean:
	rm -f *.o $(bin) rfs.h rfs.*.c

$(bin):
	$(C99) -o $@ $^ $(LDFLAGS) $(LIBS)

rfs: rfsc.o rfsc_ops.o rfs.client.o
rfsc.o rfsc_ops.o: rfsc.h rfs.h

rfsd: rfsd.o rfsd_ops.o rfs.server.o
rfsd.o rfsd_ops.o: rfsd.h rfs.h

rfsc.o rfsc_ops.o: CFLAGS += $(shell pkg-config --cflags fuse)
rfs: LDFLAGS += $(shell pkg-config --libs fuse)

%.o: %.c
	$(C99) $(CFLAGS) -c -o $@ $<

include $(IPCDIR)/ipc.mk
