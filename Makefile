all: build/debug/Makefile build/release/Makefile
	make -C build/debug
	make -C build/release

install: build/release/Makefile
	make -C build/release install

install-dbg: build/release/Makefile
	make -C build/debug install

clean:
	rm -rf build install install-dbg

build/debug/Makefile: | build/debug
	cd build/debug; \
	  cmake -DCMAKE_BUILD_TYPE=Debug \
	  -DCMAKE_INSTALL_PREFIX=$$PWD/../../install-dbg ../..

build/release/Makefile: | build/release
	cd build/release; \
	  cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
	  -DCMAKE_INSTALL_PREFIX=$$PWD/../../install ../..

build/debug:
	mkdir -p $@

build/release:
	mkdir -p $@
