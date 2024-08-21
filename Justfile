set positional-arguments

run +args: build-c
  go run go/*.go "$@"

build-c: ensure-output
  cd build && gcc -O3 -Werror \
    -Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wfloat-equal -Wcast-align \
    -Wstrict-prototypes -Wwrite-strings -Waggregate-return -Wcast-qual \
    -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code \
    -Wuninitialized -Winit-self \
    -c ../c/*.c
  ar -rcs build/libcgoboy.a build/*.o
  cp c/*.h build/

ensure-output:
  rm -rf build
  mkdir build
