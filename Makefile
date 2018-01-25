CC      = $(CROSS_COMPILE)g++
CFLAGS  +=  -c -Wall $(EXTRAFLAGS)
LDFLAGS +=  -lcurl
DZ_OBJ  = $(patsubst %.cpp,%.o,$(wildcard domoticzclient/*.cpp))
EV_OBJ  = $(patsubst %.cpp,%.o,$(wildcard evohomeclient/*.cpp)) $(patsubst %.cpp,%.o,$(wildcard evohomeclient/jsoncpp/*.cpp))
DEPS    = $(wildcard app/evo-client/*.h) $(wildcard evohomeclient/*.h) $(wildcard evohomeclient/jsoncpp/*.h) $(wildcard domoticzclient/*.h) $(wildcard demo/*.h)


all: evo-client

demo: evo-demo evo-update evo-schedule-backup evo-setmode evo-settemp evo-nodomo

evo-client: app/evo-client/evo-client.o $(EV_OBJ) $(DZ_OBJ)
	$(CC) app/evo-client/evo-client.o $(EV_OBJ) $(DZ_OBJ) $(LDFLAGS) -o evo-client

evo-demo: demo/evo-demo.o $(EV_OBJ) $(DZ_OBJ)
	$(CC) demo/evo-demo.o $(EV_OBJ) $(DZ_OBJ) $(LDFLAGS) -o evo-demo

evo-update: demo/evo-update.o $(EV_OBJ) $(DZ_OBJ)
	$(CC) demo/evo-update.o $(EV_OBJ) $(DZ_OBJ) $(LDFLAGS) -o evo-update

evo-schedule-backup: demo/evo-schedule-backup.o $(EV_OBJ)
	$(CC) demo/evo-schedule-backup.o $(EV_OBJ) $(LDFLAGS) -o evo-schedule-backup

evo-setmode: demo/evo-setmode.o $(EV_OBJ)
	$(CC) demo/evo-setmode.o $(EV_OBJ) $(LDFLAGS) -o evo-setmode

evo-settemp: demo/evo-settemp.o $(EV_OBJ) $(DZ_OBJ)
	$(CC) demo/evo-settemp.o $(EV_OBJ) $(DZ_OBJ) $(LDFLAGS) -o evo-settemp

evo-nodomo: demo/evo-nodomo.o $(EV_OBJ) $(DZ_OBJ)
	$(CC) demo/evo-nodomo.o $(EV_OBJ) $(DZ_OBJ) $(LDFLAGS) -o evo-nodomo

%.o: %.cpp $(DEP)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $< -o $@

distclean: clean

clean:
	rm -f $(EV_OBJ) $(DZ_OBJ) $(wildcard app/evo-client/*.o) $(wildcard demo/*.o) evo-demo evo-update evo-schedule-backup evo-setmode evo-settemp evo-client evo-nodomo

