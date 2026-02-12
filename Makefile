.PHONY: configure build run clean

configure:
	cmake -S . -B build

build: configure
	cmake --build build

run: build
	./build/hello

clean:
	rm -rf build
