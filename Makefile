CC      = $(CROSS_COMPILE)g++
CFLAGS  +=  -c -Wall $(EXTRAFLAGS)
LDFLAGS +=  -lcurl -ljson-c
OBJ     = $(patsubst %.c,%.o,$(wildcard evohomeclient/*.c)) $(patsubst %.c,%.o,$(wildcard domoticzclient/*.c))
DEPS    = $(wildcard app/evo-client/*.h) $(wildcard evohomeclient/*.h) $(wildcard domoticzclient/*.h) $(wildcard demo/*.h)


all: evo-client

demo: evo-demo evo-update evo-schedule-backup evo-setmode evo-settemp

evo-client: app/evo-client/evo-client.o $(OBJ)
	$(CC) app/evo-client/evo-client.o $(OBJ) $(LDFLAGS) -o evo-client

evo-demo: $(OBJ) demo/evo-demo.o
	$(CC) demo/evo-demo.o $(OBJ) $(LDFLAGS) -o evo-demo

evo-update: demo/evo-update.o $(OBJ)
	$(CC) demo/evo-update.o $(OBJ) $(LDFLAGS) -o evo-update

evo-schedule-backup: demo/evo-schedule-backup.o evohomeclient/evohomeclient.o evohomeclient/webclient.o
	$(CC) demo/evo-schedule-backup.o evohomeclient/evohomeclient.o evohomeclient/webclient.o $(LDFLAGS) -o evo-schedule-backup

evo-setmode: demo/evo-setmode.o evohomeclient/evohomeclient.o evohomeclient/webclient.o
	$(CC) demo/evo-setmode.o evohomeclient/evohomeclient.o evohomeclient/webclient.o $(LDFLAGS) -o evo-setmode

evo-settemp: demo/evo-settemp.o $(OBJ)
	$(CC) demo/evo-settemp.o $(OBJ) $(LDFLAGS) -o evo-settemp

%.o: %.c $(DEP)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $< -o $@

distclean: clean

clean:
	rm -f $(OBJ) $(wildcard app/evo-client/*.o) $(wildcard demo/*.o) evo-demo evo-update evo-schedule-backup evo-setmode evo-settemp evo-client

