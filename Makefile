CC      = g++
CFLAGS  +=  -c -Wall
LDFLAGS +=  -lcurl -ljson-c
OBJ     = $(patsubst %.c,%.o,$(wildcard lib/*.c))
DEPS    = $(wildcard src/*.h) $(wildcard lib/*.h)

all: evo-demo evo-update evo-schedule-backup


evo-demo: $(OBJ) src/evo-demo.o
	$(CC) src/evo-demo.o $(OBJ) $(LDFLAGS) -o evo-demo

evo-update: src/evo-update.o lib/base64.o lib/domoticzclient.o lib/evohomeclient.o lib/webclient.o
	$(CC) src/evo-update.o lib/base64.o lib/domoticzclient.o lib/evohomeclient.o lib/webclient.o $(LDFLAGS) -o evo-update

evo-schedule-backup: src/evo-schedule-backup.o lib/evohomeclient.o lib/webclient.o
	$(CC) src/evo-schedule-backup.o lib/evohomeclient.o lib/webclient.o $(LDFLAGS) -o evo-schedule-backup

%.o: %.c $(DEP)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $< -o $@

distclean: clean

clean:
	rm -f $(OBJ) evo-demo evo-update evo-schedule-backup $(wildcard src/*.o)

