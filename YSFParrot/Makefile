CC      = cc
CXX     = c++
CFLAGS  = -g -O3 -Wall -std=c++17 -Wno-psabi -MMD -MD -pthread
LIBS    = -lpthread
LDFLAGS = -g

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

all:		YSFParrot

YSFParrot:	$(OBJS)
		$(CXX) $(OBJS) $(CFLAGS) $(LIBS) -o YSFParrot

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<
-include $(DEPS)

YSFParrot.o: GitVersion.h FORCE

.PHONY: GitVersion.h

FORCE:

install:
		install -m 755 YSFParrot /usr/local/bin/

clean:
		$(RM) YSFParrot *.o *.d *.bak *~ GitVersion.h
 
GitVersion.h:
ifneq ("$(wildcard ../.git/index)","")
	echo "const char *gitversion = \"$(shell git rev-parse HEAD)\";" > $@
else
	echo "const char *gitversion = \"0000000000000000000000000000000000000000\";" > $@
endif
