CC      = $(CROSS_COMPILE)g++
CFLAGS  +=  -c -Wall $(EXTRAFLAGS)
LDFLAGS +=  -lcurl -ljson-c
OBJ     = $(patsubst %.c,%.o,$(wildcard lib/*.c))
DEPS    = $(wildcard src/*.h) $(wildcard lib/*.h) $(wildcard demo/*.h)


all: evo-client

demo: evo-demo evo-update evo-schedule-backup evo-setmode evo-settemp

evo-client: src/evo-client.o lib/base64.o lib/domoticzclient.o lib/evohomeclient.o lib/webclient.o
	$(CC) src/evo-client.o lib/base64.o lib/domoticzclient.o lib/evohomeclient.o lib/webclient.o $(LDFLAGS) -o evo-client

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
	rm -f $(OBJ) $(wildcard src/*.o) $(wildcard demo/*.o) evo-demo evo-update evo-schedule-backup evo-setmode evo-settemp evo-client

