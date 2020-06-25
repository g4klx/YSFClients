CC      = gcc
CXX     = g++
CFLAGS  = -g -O3 -Wall -std=c++0x -pthread
LIBS    = -lpthread
LDFLAGS = -g

OBJECTS = Network.o Parrot.o StopWatch.o Thread.o Timer.o UDPSocket.o YSFParrot.o

all:		YSFParrot

YSFParrot:	$(OBJECTS)
		$(CXX) $(OBJECTS) $(CFLAGS) $(LIBS) -o YSFParrot

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<

install:
		install -m 755 YSFParrot /usr/local/bin/

clean:
		$(RM) YSFParrot *.o *.d *.bak *~
 
