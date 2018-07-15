CC=g++
CXX=g++
RANLIB=ranlib

LIBSRC=MapReduceFramework.cpp Barrier.cpp Barrier.h
LIBOBJ=$(LIBSRC:.cpp=.o)

INCS=-I.
CFLAGS = -Wall -std=c++11 -g $(INCS)
CXXFLAGS = -Wall -std=c++11 -g $(INCS)

OSMLIB = libMapReduceFramework.a
TARGETS = $(OSMLIB)

TAR=tar
TARFLAGS=-cvf
TARNAME=ex3.tar
TARSRCS=$(LIBSRC) Makefile README
OUTPUT=SampleClient.o

all: $(TARGETS)

$(TARGETS): $(LIBOBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@
.PHONY: clean

clean:
	 rm -f $(binaries) *.o *.a

depend:
	makedepend -- $(CFLAGS) -- $(SRC) $(LIBSRC)
client:
	$(CC) -std=c++11 -pthread -g SampleClient.cpp libMapReduceFramework.a -o $(OUTPUT)
	
tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
