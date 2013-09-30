

.phony: all clean cp lp simd dyser ccores beret

all: cp simd dyser ccores beret

cp:: lp
	+make -C critpath

lp::
	+make -C loopprof

simd:
	+make -C critpath/simd

dyser:
	+make -C critpath/dyser

ccores:
	+make -C critpath/ccores

beret:
	+make -C critpath/beret

clean:
	+make -C loopprof clean
	+make -C critpath clean
	+make -C critpath/simd clean
	+make -C critpath/dyser clean
	+make -C critpath/ccores clean
	+make -C critpath/beret clean
