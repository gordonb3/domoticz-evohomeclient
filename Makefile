CC      = g++
CFLAGS  +=  -c -Wall
LDFLAGS +=  -lcurl -ljson-c
OBJ     = $(patsubst %.c,%.o,$(wildcard src/*.c)) $(patsubst %.c,%.o,$(wildcard lib/*.c))
DEPS    = $(wildcard src/*.h) $(wildcard lib/*.h)

all: evo-demo

evo-demo: $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o evo-demo

%.o: %.c $(DEP)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $< -o $@

distclean: clean

clean:
	rm -f $(OBJ) evo-demo

install: all
	install -d -m 0755 $(DESTDIR)/usr/bin
	install -m 0755 ef $(DESTDIR)/usr/bin

uninstall:
	rm -f $(DESTDIR)/usr/bin/evo-demo
	rmdir --ignore-fail-on-non-empty -p $(DESTDIR)/usr/bin


