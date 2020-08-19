CC      = gcc
CXX     = g++

# Use the following CFLAGS and LIBS if you don't want to use gpsd.
CFLAGS  = -g -O3 -Wall -std=c++0x -pthread
LIBS    = -lm -lpthread

# Use the following CFLAGS and LIBS if you do want to use gpsd.
#CFLAGS  = -g -O3 -Wall -DUSE_GPSD -std=c++0x -pthread
#LIBS    = -lm -lpthread -lgps

LDFLAGS = -g

OBJECTS = APRSWriter.o Conf.o CRC.o DGIdGateway.o DGIdNetwork.o FCSNetwork.o Golay24128.o GPS.o Log.o StopWatch.o Sync.o Thread.o Timer.o \
	  UDPSocket.o Utils.o YSFConvolution.o YSFFICH.o YSFNetwork.o YSFPayload.o YSFReflectors.o

all:		DGIdGateway

DGIdGateway:	$(OBJECTS)
		$(CXX) $(OBJECTS) $(CFLAGS) $(LIBS) -o DGIdGateway

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<

install:
		install -m 755 DGIdGateway /usr/local/bin/

clean:
		$(RM) DGIdGateway *.o *.d *.bak *~
 
