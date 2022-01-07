CC      = cc
CXX     = c++

# Use the following CFLAGS and LIBS if you don't want to use gpsd.
CFLAGS  = -g -O3 -Wall -DHAVE_LOG_H -std=c++0x -pthread
LIBS    = -lm -lpthread

# Use the following CFLAGS and LIBS if you do want to use gpsd.
#CFLAGS  = -g -O3 -Wall -DHAVE_LOG_H -DUSE_GPSD -std=c++0x -pthread
#LIBS    = -lm -lpthread -lgps

LDFLAGS = -g

OBJECTS = APRSWriter.o Conf.o CRC.o DGIdGateway.o DGIdNetwork.o FCSNetwork.o Golay24128.o GPS.o IMRSNetwork.o \
	  Log.o StopWatch.o Sync.o Thread.o Timer.o UDPSocket.o Utils.o YSFConvolution.o YSFFICH.o YSFNetwork.o \
	  YSFPayload.o YSFReflectors.o

all:		DGIdGateway

DGIdGateway:	$(OBJECTS)
		$(CXX) $(OBJECTS) $(CFLAGS) $(LIBS) -o DGIdGateway

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<

DGIdGateway.o: GitVersion.h FORCE

.PHONY: GitVersion.h

FORCE:

install:
		install -m 755 DGIdGateway /usr/local/bin/

clean:
		$(RM) DGIdGateway *.o *.d *.bak *~ GitVersion.h

GitVersion.h:
ifneq ("$(wildcard ../.git/index)","")
	echo "const char *gitversion = \"$(shell git rev-parse HEAD)\";" > $@
else
	echo "const char *gitversion = \"0000000000000000000000000000000000000000\";" > $@
endif

