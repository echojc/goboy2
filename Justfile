run-go: build-c
  go run go/*.go

build-c: ensure-output
  gcc -O3 -Werror \
    -Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wfloat-equal -Wcast-align \
    -Wstrict-prototypes -Wwrite-strings -Waggregate-return -Wcast-qual \
    -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code \
    -Wuninitialized -Winit-self \
    -c -o build/cgoboy.o c/*.c
  ar -rcs build/libcgoboy.a build/cgoboy.o
  cp c/*.h build/

ensure-output:
  rm -rf build
  mkdir build
