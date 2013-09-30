

.phony: all clean critpath loopprof

all: critpath simd dyser

critpath:: loopprof
	+make -C critpath

loopprof::
	+make -C loopprof

simd:
	+make -C critpath/simd

dyser:
	+make -C critpath/dyser

clean:
	+make -C loopprof clean
	+make -C critpath clean
	+make -C critpath/simd clean
	+make -C critpath/dyser clean
