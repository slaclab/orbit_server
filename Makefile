CC = gcc
CCX = g++
CFLAGS = -g -Wall -std=c++11
TARGET = orbitserver

INCLUDES = -I${EPICS_BASE}/include -I${EPICS_BASE}/include/os/Linux -I${EPICS_BASE}/include/os/Darwin
LFLAGS = -L${EPICS_BASE}/lib/${EPICS_HOST_ARCH}
LIBS = -lca -lCom

all: $(TARGET)

$(TARGET): orbit.o
	$(CXX) $(CFLAGS) $(INCLUDES) $(LFLAGS) $(LIBS) -o $(TARGET) orbit.o 

orbit.o: orbit.cpp orbit.h
	$(CCX) $(CFLAGS) $(INCLUDES) $(LFLAGS) $(LIBS) -c orbit.cpp
clean:
	$(RM) $(TARGET) *.o *~
