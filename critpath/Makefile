
LEVEL = ./

SRC := critpath.cpp gzstream.cpp prof.cpp plugin.cpp origcp.cpp exec_profile.cpp

OBJ := $(patsubst %.cpp, %.o, $(SRC))

LIBS := -lz -L../loopprof/. -lloopprof -lboost_serialization -ldl
LDFLAGS := $(EXFLAGS) -ltcmalloc \
                -L/s/boost-1.54.0/lib \
		-L/unsup/gperftools-2.0/amd64_rhel6/lib/ \
                -Wl,-rpath,/s/boost-1.54.0/lib \
		-Wl,-rpath,/unsup/gperftools-2.0/amd64_rhel6/lib/ \
		-Wl,-rpath,$(shell pwd)/../loopprof -rdynamic

BINARY = critpath

include $(LEVEL)/Makefile.rules

-include $(OBJ:.o=.d)
