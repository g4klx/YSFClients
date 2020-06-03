CC      = gcc
CXX     = g++
CFLAGS  = -g -O3 -Wall -std=c++0x -pthread
LIBS    = -lm -lpthread -lgps
LDFLAGS = -g

OBJECTS = APRSWriter.o Conf.o CRC.o DTMF.o FCSNetwork.o Golay24128.o GPS.o Log.o StopWatch.o Sync.o Thread.o Timer.o \
	  UDPSocket.o Utils.o WiresX.o YSFConvolution.o YSFFICH.o YSFGateway.o YSFNetwork.o YSFPayload.o YSFReflectors.o

all:		YSFGateway

YSFGateway:	$(OBJECTS)
		$(CXX) $(OBJECTS) $(CFLAGS) $(LIBS) -o YSFGateway

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<

clean:
		$(RM) YSFGateway *.o *.d *.bak *~
 
