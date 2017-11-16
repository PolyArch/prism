TARGET = mcpat
SHELL = /bin/sh
.PHONY: all depend clean
.SUFFIXES: .cc .o

ifndef NTHREADS
  NTHREADS = 4
endif


LIBS = 
INCS = -static -lm -I/usr/include/boost -L/usr/lib64 -lboost_serialization -Wl,-rpath,/usr/lib64

ifeq ($(TAG),dbg)
  DBG = -Wall 
  OPT = -ggdb -g -fPIC -gdwarf-3  -O0 -DNTHREADS=1 -Icacti
else
  DBG = 
  OPT =  -ggdb -g -fPIC -gdwarf-3 -fPIC -O3 -msse2 -mfpmath=sse -DNTHREADS=$(NTHREADS) -Icacti
  #OPT = -O0 -DNTHREADS=$(NTHREADS)
endif

#CXXFLAGS = -Wall -Wno-unknown-pragmas -Winline $(DBG) $(OPT) 
CXXFLAGS = -static -Wno-unknown-pragmas $(DBG) -std=c++11 $(OPT) 
#CXX = /s/gcc-4.8.2/bin/g++
#CC  = /s/gcc-4.8.2/bin/gcc
#CXX = /s/gcc-4.8.2/bin/g++ -m32
#CC  = /s/gcc-4.8.2/bin/gcc -m32
CC = gcc
CXX = g++

VPATH = cacti

SRCS  = \
  Ucache.cc \
  XML_Parse.cc \
  arbiter.cc \
  area.cc \
  array.cc \
  bank.cc \
  basic_circuit.cc \
  basic_components.cc \
  cacti_interface.cc \
  component.cc \
  core.cc \
  crossbar.cc \
  decoder.cc \
  htree2.cc \
  interconnect.cc \
  io.cc \
  iocontrollers.cc \
  logic.cc \
  mat.cc \
  memoryctrl.cc \
  noc.cc \
  nuca.cc \
  parameter.cc \
  processor.cc \
  router.cc \
  sharedcache.cc \
  subarray.cc \
  technology.cc \
  uca.cc \
  wire.cc \
  xmlParser.cc \
  powergating.cc

FULL_SRCS = \
  $(SRCS) \
  main.cc \


OBJS = $(patsubst %.cc,obj_$(TAG)/%.o,$(SRCS))

FULL_OBJS = $(patsubst %.cc,obj_$(TAG)/%.o,$(FULL_SRCS))

all: obj_$(TAG)/$(TARGET)
	cp -f obj_$(TAG)/$(TARGET) $(TARGET)

obj_$(TAG)/$(TARGET) : $(FULL_OBJS)
	#$(CXX) -r $(OBJS) -o mcpat_glob.o $(INCS) $(CXXFLAGS) $(LIBS) -pthread
	ar -rsc mcpat.a $(OBJS)
	$(CXX) $(FULL_OBJS) -o $@ $(INCS) $(CXXFLAGS) $(LIBS) -pthread

#obj_$(TAG)/%.o : %.cc
#	$(CXX) -c $(CXXFLAGS) $(INCS) -o $@ $<

obj_$(TAG)/%.o : %.cc
	$(CXX) $(CXXFLAGS) $(INCS) -c $< -o $@

clean:
	-rm -f *.o $(TARGET)


