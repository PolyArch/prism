
LEVEL = ./

SRC := critpath.cpp gzstream.cpp prof.cpp plugin.cpp origcp.cpp exec_profile.cpp

OBJ := $(patsubst %.cpp, %.o, $(SRC))

EXT_OBJ := mcpat/mcpat.a

LIBS := -lpthread -lz  -lloopprof -lboost_serialization -ldl -ltcmalloc -lprofiler

# ifndef USE_GLIBC_MALLOC
# LDFLAGS := $(EXFLAGS) -ltcmalloc 
#                 -L/s/boost-1.54.0/lib 
# 		-L/unsup/gperftools-2.0/amd64_rhel6/lib/ 
#                 -Wl,-rpath,/s/boost-1.54.0/lib 
# 		-Wl,-rpath,/unsup/gperftools-2.0/amd64_rhel6/lib/ 
# 		-Wl,-rpath,$(shell pwd)/../loopprof -rdynamic
# else
LDFLAGS := $(EXFLAGS) -L../loopprof/ \
                -L/s/boost-1.54.0/lib \
		-L/unsup/gperftools-2.0/amd64_rhel6/lib/ \
		-L/unsup/gperftools-2.0/lib \
		-Wl,-rpath,$(shell pwd)/../loopprof -rdynamic \
		-Wl,-rpath,/unsup/gperftools-2.0/lib/ \
                -Wl,-rpath,/s/boost-1.54.0/lib 

#                -L/unsup/boost-1.58.0/lib \
#                -Wl,-rpath,/unsup/boost-1.58.0/lib \


#endif

BINARY = critpath


include $(LEVEL)/Makefile.rules

-include $(OBJ:.o=.d)
