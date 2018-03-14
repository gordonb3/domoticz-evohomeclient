CC      = $(CROSS_COMPILE)g++
CFLAGS  +=  -c -Wall $(EXTRAFLAGS)
LDFLAGS +=  -lcurl
DZ_OBJ  = $(patsubst %.cpp,%.o,$(wildcard domoticzclient/*.cpp))
EV_OBJ  = $(patsubst %.cpp,%.o,$(wildcard evohomeclient/*.cpp)) $(patsubst %.cpp,%.o,$(wildcard evohomeclient/jsoncpp/*.cpp))
DEPS    = $(wildcard app/evo-client/*.h) $(wildcard evohomeclient/*.h) $(wildcard evohomeclient/jsoncpp/*.h) $(wildcard domoticzclient/*.h)


all: evo-client


evo-client: app/evo-client/evo-client.o $(EV_OBJ) $(DZ_OBJ)
	$(CC) app/evo-client/evo-client.o $(EV_OBJ) $(DZ_OBJ) $(LDFLAGS) -o evo-client

%.o: %.cpp $(DEP)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $< -o $@

distclean: clean

clean:
	rm -f $(EV_OBJ) $(DZ_OBJ) $(wildcard app/evo-client/*.o) evo-client

