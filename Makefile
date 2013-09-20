

.phony: all clean critpath loopprof

all: critpath

critpath:: loopprof
	+make -C critpath

loopprof::
	+make -C loopprof


clean:
	+make -C loopprof clean
	+make -C critpath clean
