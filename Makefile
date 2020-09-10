CC = gcc
CCX = g++
CFLAGS = -g -Wall -std=c++11 -pthread
TARGET = orbitserver

INCLUDES = -I${EPICS_BASE}/include -I${EPICS_BASE}/include/os/Linux -I${EPICS_BASE}/include/os/Darwin -I${EPICS_BASE}/include/compiler/clang -I${EPICS_BASE}/include/compiler/gcc -I./pvxs/bundle/usr/${EPICS_HOST_ARCH}/include -I./pvxs/include
LFLAGS = -L${EPICS_BASE}/lib/${EPICS_HOST_ARCH} -L./pvxs/lib/${EPICS_HOST_ARCH} -L./pvxs/bundle/usr/${EPICS_HOST_ARCH}/lib
LIBS = -lca -lCom -lpvxs -lpvData -levent

all: $(TARGET)

$(TARGET): main.o pva_orbit_receiver.o orbit.o pv.o
	$(CXX) $(CFLAGS) $(INCLUDES) $(LFLAGS) $(LIBS) -o $(TARGET) main.o pva_orbit_receiver.o orbit.o pv.o

main.o: main.cpp
	$(CCX) $(CFLAGS) $(INCLUDES) -c main.cpp

pva_orbit_receiver.o: pva_orbit_receiver.cpp pva_orbit_receiver.h
	$(CCX) $(CFLAGS) $(INCLUDES) -c pva_orbit_receiver.cpp

orbit.o: orbit.cpp orbit.h
	$(CCX) $(CFLAGS) $(INCLUDES) -c orbit.cpp
	
bpm.o: bpm.cpp bpm.h
	$(CCX) $(CFLAGS) $(INCLUDES) -c bpm.cpp

pv.o: pv.cpp pv.h
	$(CCX) $(CFLAGS) $(INCLUDES) -c pv.cpp
	
clean:
	$(RM) $(TARGET) *.o *~
