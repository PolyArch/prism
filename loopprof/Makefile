#CC := /s/gcc-4.8.2/bin/gcc
#CXX := /s/gcc-4.8.2/bin/g++
#CC  := /s/gcc-5.1/bin/gcc
#CXX := /s/gcc-5.1/bin/g++
CC := gcc
CXX := g++

OPTI = -O3

GEM5_DIR=../../gem5
-include ../make.config

#boost_dir=/s/boost-1.54.0
#boost_dir=/unsup/boost-1.58.0
boost_dir=/usr/

SRC := pathprof.cpp gzstream.cpp op.cpp bb.cpp loopinfo.cpp functioninfo.cpp lpanalysis.cpp
CFLAGS :=  -std=c++11 ${OPTI} -ggdb -gdwarf-3 -g -I. -Wall -Werror -Wno-unused-local-typedefs -Wno-deprecated-declarations  -pedantic -fPIC -rdynamic -I$(GEM5_DIR)/src  -I$(GEM5_DIR)/src -DSTANDALONE_CRITPATH -I${boost_dir}/include -L${boost_dir}/lib -I../include 
LIBS := -lz -I${boost_dir}/include -L${boost_dir}/lib -lboost_serialization -I/unsup/gperftools-2.0/inc -L/unsup/gperftools-2.0/lib -lprofiler 
OBJ := $(patsubst %.cpp, %.o, $(SRC))
#LDFLAGS := $(EXFLAGS) -ltcmalloc_minimal -L/unsup/gperftools-2.0/amd64_rhel6/lib/ -Wl,-rpath,/unsup/gperftools-2.0/amd64_rhel6/lib/ -L. -Wl,-rpath,$(shell pwd)
#LDFLAGS := $(EXFLAGS) -ltcmalloc -L/unsup/gperftools-2.0/amd64_rhel6/lib/ -Wl,-rpath,/unsup/gperftools-2.0/amd64_rhel6/lib/ -L. -Wl,-rpath,$(shell pwd) -Wl,-rpath,/s/boost-1.54.0/lib 
LDFLAGS := $(EXFLAGS)  -L. -Wl,-rpath,$(shell pwd) -Wl,-rpath,${boost_dir}/lib  -Wl,-rpath,/unsup/gperftools-2.0/lib/ 


quiet	= quiet_

quiet_cmd_cc_o_c = "	[CC]			$(notdir $<)"
	cmd_cc_o_c	 = $(CC) $(CFLAGS) -MD -c -o $@ $<

quiet_cmd_cxx_o_cxx = "	[CC]			$(notdir $<)"
	cmd_cxx_o_cxx	 = $(CXX) $(CFLAGS) -MD -c -o $@ $<

quiet_cmd_ccld_out_o = "	[CCLD]			$@"
	cmd_ccld_out_o	 = $(CXX) $(LDFLAGS) -shared -o $@ $(OBJ) $(LIBS) 

quiet_cmd_ccld_out_o_driver = "	[CC_DRIVER]			$@"
	cmd_ccld_out_o_driver	 = $(CXX) $(CFLAGS) $(LDFLAGS) $(SHLDFLAGS) -o $@ $<  $(LIBS) -lloopprof

all: loopprof 
	#archive-test

#archive-test: archive-test.cpp libloopprof.so
#	@echo $($(quiet)cmd_ccld_out_o_driver)
#	@$(cmd_ccld_out_o_driver)

loopprof: loopprof.cpp libloopprof.so ../critpath/exec_profile.cpp
	@echo $($(quiet)cmd_ccld_out_o_driver) ../critpath/exec_profile.cpp
	@$(cmd_ccld_out_o_driver)  ../critpath/exec_profile.cpp

libloopprof.so: $(OBJ)  *.hh
	@echo $($(quiet)cmd_ccld_out_o)
	@$(cmd_ccld_out_o)

%.o : %.cpp
	@echo $($(quiet)cmd_cxx_o_cxx)
	@$(cmd_cxx_o_cxx)

%.o : %.c
	@echo $($(quiet)cmd_cc_o_c)
	@$(cmd_cc_o_c)

.phony: clean all
clean:
	-rm -f loopprof archive-test *.o *.d *.so

-include $(OBJ:.o=.d)
