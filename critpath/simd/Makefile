
CC :=  /s/gcc-4.8.1/bin/gcc
CXX := /s/gcc-4.8.1/bin/g++

GEM5_DIR = /p/vertical/afs-huge0/MaFIA/src/gem5
OPT = -O0
-include ../../make.config

CFLAGS := $(OPT)  -gdwarf-3  -I../ -I$(GEM5_DIR)/src -std=c++11 -I../../loopprof -fPIC

quiet_cmd_cc_o_c = "	[CC]			$(notdir $<)"
	cmd_cc_o_c	 = $(CC) $(CFLAGS) -MD -c -o $@ $<

quiet_cmd_cxx_o_cxx = "	[CC]			$(notdir $<)"
	cmd_cxx_o_cxx	 = $(CXX) $(CFLAGS) -MD -c -o $@ $<

quiet_cmd_ccld_out_so = "	[CCLD]  -shared		$@"
	cmd_ccld_out_so	 = $(CXX)  $(LDFLAGS) -shared -fPIC  -o $@ $(OBJ) $(LIBS)


OBJ = cp_simd.o exec_profile.o

install: libcpsimd.so ../plugin/
	cp libcpsimd.so ../plugin/

libcpsimd.so : $(OBJ) *.hh
	@echo $($(quiet)cmd_ccld_out_so)
	@$(cmd_ccld_out_so)

../plugin:
	mkdir -p ../plugin

%.o : %.cpp *.hh
	@echo $($(quiet)cmd_cxx_o_cxx)
	@$(cmd_cxx_o_cxx)

.phony: clean
clean:
	-rm -f *.o *.so
