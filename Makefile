CC      = g++
CFLAGS  +=  -c -Wall
LDFLAGS +=  -lcurl -ljson-c
OBJ     = $(patsubst %.c,%.o,$(wildcard lib/*.c))
DEPS    = $(wildcard src/*.h) $(wildcard lib/*.h)

all: evo-demo evo-update


evo-update: src/evo-update.o lib/base64.o lib/domoticzclient.o lib/evohomeclient.o lib/webclient.o
	$(CC) src/evo-update.o lib/base64.o lib/domoticzclient.o lib/evohomeclient.o lib/webclient.o $(LDFLAGS) -o evo-update

evo-demo: $(OBJ) src/evo-demo.o
	$(CC) src/evo-demo.o $(OBJ) $(LDFLAGS) -o evo-demo

%.o: %.c $(DEP)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $< -o $@

distclean: clean

clean:
	rm -f $(OBJ) src/evo-demo.o src/evo-update.o evo-demo evo-update

