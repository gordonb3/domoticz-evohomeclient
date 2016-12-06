CC      = g++
CFLAGS  +=  -c -Wall
LDFLAGS +=  -lcurl -ljson-c
OBJ     = $(patsubst %.c,%.o,$(wildcard lib/*.c))
DEPS    = $(wildcard demo/*.h) $(wildcard lib/*.h)

all: evo-demo evo-update evo-schedule-backup evo-setmode evo-settemp


evo-demo: $(OBJ) demo/evo-demo.o
	$(CC) demo/evo-demo.o $(OBJ) $(LDFLAGS) -o evo-demo

evo-update: demo/evo-update.o lib/base64.o lib/domoticzclient.o lib/evohomeclient.o lib/webclient.o
	$(CC) demo/evo-update.o lib/base64.o lib/domoticzclient.o lib/evohomeclient.o lib/webclient.o $(LDFLAGS) -o evo-update

evo-schedule-backup: demo/evo-schedule-backup.o lib/evohomeclient.o lib/webclient.o
	$(CC) demo/evo-schedule-backup.o lib/evohomeclient.o lib/webclient.o $(LDFLAGS) -o evo-schedule-backup

evo-setmode: demo/evo-setmode.o lib/evohomeclient.o lib/webclient.o
	$(CC) demo/evo-setmode.o lib/evohomeclient.o lib/webclient.o $(LDFLAGS) -o evo-setmode

evo-settemp: demo/evo-settemp.o lib/base64.o lib/domoticzclient.o lib/evohomeclient.o lib/webclient.o
	$(CC) demo/evo-settemp.o lib/base64.o lib/domoticzclient.o lib/evohomeclient.o lib/webclient.o $(LDFLAGS) -o evo-settemp

%.o: %.c $(DEP)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $< -o $@

distclean: clean

clean:
	rm -f $(OBJ) $(wildcard demo/*.o) evo-demo evo-update evo-schedule-backup evo-setmode evo-settemp

