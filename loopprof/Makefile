CC := /s/gcc-4.8.1/bin/gcc
CXX := /s/gcc-4.8.1/bin/g++

OPT ?= -O3

GEM5_DIR=/p/vertical/afs-huge0/MaFIA/src/gem5
-include ../make.config


SRC := pathprof.cpp gzstream.cpp op.cpp bb.cpp loopinfo.cpp functioninfo.cpp
CFLAGS :=  -std=c++11 ${OPT} -ggdb -gdwarf-3 -g -I. -Wall -Werror -pedantic -fPIC -rdynamic -I$(GEM5_DIR)/src  -I$(GEM5_DIR)/src
LIBS := -lz -lboost_serialization
OBJ := $(patsubst %.cpp, %.o, $(SRC))
#LDFLAGS := $(EXFLAGS) -ltcmalloc_minimal -L/unsup/gperftools-2.0/amd64_rhel6/lib/ -Wl,-rpath,/unsup/gperftools-2.0/amd64_rhel6/lib/ -L. -Wl,-rpath,$(shell pwd)
LDFLAGS := $(EXFLAGS) -ltcmalloc -L/unsup/gperftools-2.0/amd64_rhel6/lib/ -Wl,-rpath,/unsup/gperftools-2.0/amd64_rhel6/lib/ -L. -Wl,-rpath,$(shell pwd)


quiet	= quiet_

quiet_cmd_cc_o_c = "	[CC]			$(notdir $<)"
	cmd_cc_o_c	 = $(CC) $(CFLAGS) -MD -c -o $@ $<

quiet_cmd_cxx_o_cxx = "	[CC]			$(notdir $<)"
	cmd_cxx_o_cxx	 = $(CXX) $(CFLAGS) -MD -c -o $@ $<

quiet_cmd_ccld_out_o = "	[CCLD]			$@"
	cmd_ccld_out_o	 = $(CXX) $(LDFLAGS) -shared -o $@ $(OBJ) $(LIBS) 

quiet_cmd_ccld_out_o_driver = "	[CC_DRIVER]			$@"
	cmd_ccld_out_o_driver	 = $(CXX) $(CFLAGS) $(LDFLAGS) $(SHLDFLAGS) -o $@ $<  $(LIBS) -lloopprof

all: loopprof archive-test

archive-test: archive-test.cpp libloopprof.so
	@echo $($(quiet)cmd_ccld_out_o_driver)
	@$(cmd_ccld_out_o_driver)

loopprof: loopprof.cpp libloopprof.so
	@echo $($(quiet)cmd_ccld_out_o_driver)
	@$(cmd_ccld_out_o_driver)

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
