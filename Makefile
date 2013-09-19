

.phony: all
all:
	make -C loopprof
	make -C critpath

clean:
	make -C loopprof clean
	make -C critpath clean
