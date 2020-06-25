CC      = gcc
CXX     = g++
CFLAGS  = -g -O3 -Wall -std=c++0x -pthread
LIBS    = -lpthread
LDFLAGS = -g

OBJECTS = Conf.o Log.o Network.o StopWatch.o Thread.o Timer.o UDPSocket.o Utils.o YSFReflector.o

all:		YSFReflector

YSFReflector:	$(OBJECTS)
		$(CXX) $(OBJECTS) $(CFLAGS) $(LIBS) -o YSFReflector

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<

install:
		install -m 755 YSFReflector /usr/local/bin/

clean:
		$(RM) YSFReflector *.o *.d *.bak *~
 
