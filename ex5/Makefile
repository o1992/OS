CC=g++
CXX=g++
RANLIB=ranlib

LIBSRC=VirtualMemory.cpp
LIBOBJ=$(LIBSRC:.cpp=.o)

INCS=-I.
CFLAGS = -Wall -std=c++11 -g $(INCS)
CXXFLAGS = -Wall -std=c++11 -g $(INCS)

VIRTUALMEMORYLIB = libVirtualMemory.a
TARGETS = $(VIRTUALMEMORYLIB)

TAR=tar
TARFLAGS=-cvf
TARNAME=ex5.tar
TARSRCS= Makefile README VirtualMemory.cpp

all: $(TARGETS)

$(TARGETS): $(LIBOBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

clean:
	$(RM) $(TARGETS) $(VIRTUALMEMORYLIB) $(OBJ) $(LIBOBJ) *~ *core $(TARNAME)

depend:
	makedepend -- $(CFLAGS) -- $(SRC) $(LIBSRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
