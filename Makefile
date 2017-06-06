CC = gcc
CCX = g++
CFLAGS = -g -Wall -std=c++11
TARGET = orbitserver

INCLUDES = -I/home/physics/mgibbs/epics/current/include -I/home/physics/mgibbs/epics/current/include/os/Linux
LFLAGS = -L/home/physics/mgibbs/epics/current/lib/linux-x86_64
LIBS = -lca -lCom

all: $(TARGET)

$(TARGET): orbit.o
	$(CXX) $(CFLAGS) $(INCLUDES) $(LFLAGS) $(LIBS) -o $(TARGET) orbit.o 

orbit.o: orbit.cpp orbit.h
	$(CCX) $(CFLAGS) $(INCLUDES) $(LFLAGS) $(LIBS) -c orbit.cpp
clean:
	$(RM) $(TARGET) *.o *~
