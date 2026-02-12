# Project description

This project attempts to create a UI system built entirely in the C language and leveraging SDL, targeting specifically macOS. It's a learning project to master the fundamentals of UI building and the C language starting from the low level, letting SDL take care of some of the foundations, but taking over the rest of it.

SDL is currently brought in as a locally cloned repository under `vendored/SDL`:

```bash
git clone https://github.com/libsdl-org/SDL.git vendored/SDL
```

This is not configured as a proper Git submodule right now; it is just a nested Git repository used as a vendored dependency for local builds.

## Configure and Build:
```
cmake -S . -B build
cmake --build build
```

## Makefile shortcuts:
```
make build    # configure + build
make run      # build + run ./build/hello
make clean    # remove build directory
```

## Run:
The executable should be in the build directory:

```
cd build
./hello
```
