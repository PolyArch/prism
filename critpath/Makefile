CC :=  /s/gcc-4.8.1/bin/gcc
CXX := /s/gcc-4.8.1/bin/g++

GEM5_DIR=/p/vertical/afs-huge0/MaFIA/src/gem5
-include ../make.config

OPT = -O3

SRC := critpath.cpp gzstream.cpp prof.cpp plugin.cpp origcp.cpp
CFLAGS := $(EXFLAGS) $(OPT) -std=c++11 -ggdb -gdwarf-3 -g3 -g -I. -Wall -Werror -pedantic -I../loopprof/. -I$(GEM5_DIR)/src
LIBS := -lz -L../loopprof/. -lloopprof -lboost_serialization -ldl
PWD := $(shell pwd)
LDFLAGS := $(EXFLAGS) -ltcmalloc -L/unsup/gperftools-2.0/amd64_rhel6/lib/ -Wl,-rpath,/unsup/gperftools-2.0/amd64_rhel6/lib/  -Wl,-rpath,$(shell pwd)/../loopprof -rdynamic

OBJ := $(patsubst %.cpp, %.o, $(SRC))

quiet	= quiet_

quiet_cmd_cc_o_c = "	[CC]			$(notdir $<)"
	cmd_cc_o_c	 = $(CC) $(CFLAGS) -MD -c -o $@ $<

quiet_cmd_cxx_o_cxx = "	[CC]			$(notdir $<)"
	cmd_cxx_o_cxx	 = $(CXX) $(CFLAGS) -MD -c -o $@ $<

quiet_cmd_ccld_out_o = "	[CCLD]			$@"
	cmd_ccld_out_o	 = $(CXX)  $(LDFLAGS) -o $@ $(OBJ) $(LIBS) -g


critpath: $(OBJ)  *.hh
	@echo $($(quiet)cmd_ccld_out_o)
	@$(cmd_ccld_out_o)

static-cp: static-critpath.o gzstream.o
	@echo $($(quiet)cmd_ccld_out_o)
	@g++ -o $@ $^ -lz -I.

dyser: plugin
	+make -C dyser

plugin:
	mkdir -p plugin

%.o : %.cpp
	@echo $($(quiet)cmd_cxx_o_cxx)
	@$(cmd_cxx_o_cxx)

%.o : %.c
	@echo $($(quiet)cmd_cc_o_c)
	@$(cmd_cc_o_c)

.phony: clean dyser
clean:
	-rm -rf critpath *.o *.d plugin
	-make -C dyser clean

-include $(OBJ:.o=.d)
