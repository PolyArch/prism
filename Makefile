

.PHONY: all clean cp lp base simd dyser ccores beret static

all: cp nla simd base dyser ccores beret super npu disasm verbose

cp:: lp
	+make -C critpath

lp::
	+make -C loopprof

nla:
	+make -C critpath/nla

base:
	+make -C critpath/base

simd:
	+make -C critpath/simd

dyser:
	+make -C critpath/dyser

ccores:
	+make -C critpath/ccores

beret:
	+make -C critpath/beret

super:
	+make -C critpath/super

disasm:
	+make -C critpath/disasm

verbose:
	+make -C critpath/verbose
npu:
	+make -C critpath/npu


clean:
	+make -C loopprof clean
	+make -C critpath clean
	+make -C critpath/simd clean
	+make -C critpath/base clean
	+make -C critpath/dyser clean
	+make -C critpath/ccores clean
	+make -C critpath/beret clean
	+make -C critpath/super clean
	+make -C critpath/disasm clean
	+make -C critpath/verbose clean
	+make -C critpath/npu clean
	+make -C critpath/nla clean
	+make -C critpath -f Makefile.static clean


static: cp nla base simd dyser ccores beret super disasm verbose npu
	+make -C critpath -f Makefile.static

#static: clean
#	+make -C loopprof
#	+make -C critpath
#	+make -C critpath -f Makefile.static
