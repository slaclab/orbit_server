CC = gcc
CCX = g++
CFLAGS = -g -Wall -std=c++11
TARGET = orbitserver

INCLUDES = -I${EPICS_BASE}/include -I${EPICS_BASE}/include/os/Linux -I${EPICS_BASE}/include/os/Darwin
LFLAGS = -L${EPICS_BASE}/lib/${EPICS_HOST_ARCH}
LIBS = -lca -lCom

all: $(TARGET)

$(TARGET): main.o orbit.o bpm.o
	$(CXX) $(CFLAGS) $(INCLUDES) $(LFLAGS) $(LIBS) -o $(TARGET) main.o orbit.o bpm.o

main.o: main.cpp
	$(CCX) $(CFLAGS) $(INCLUDES) -c main.cpp

orbit.o: orbit.cpp orbit.h
	$(CCX) $(CFLAGS) $(INCLUDES) -c orbit.cpp
	
bpm.o: bpm.cpp bpm.h
	$(CCX) $(CFLAGS) $(INCLUDES) -c bpm.cpp
	
clean:
	$(RM) $(TARGET) *.o *~
