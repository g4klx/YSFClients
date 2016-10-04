CC      = gcc
CXX     = g++
CFLAGS  = -g -O3 -Wall -std=c++0x
LIBS    =
LDFLAGS = -g

OBJECTS = Log.o Network.o Parrot.o StopWatch.o Timer.o UDPSocket.o Utils.o YSFParrot.o

all:		YSFParrot

YSFParrot:	$(OBJECTS)
		$(CXX) $(OBJECTS) $(CFLAGS) $(LIBS) -o YSFParrot

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<

clean:
		$(RM) YSFParrot *.o *.d *.bak *~
 