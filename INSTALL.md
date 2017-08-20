# libc4 - Installation

Currently the build method is a simple `Makefile`. We're working on updating to CMake for broader support. Please open an issue if you can help.

## Dependencies 
`libc4` requires the following libraries be available at compile time to build and link the static library.

`libgmp` (gmp)[https://gmplib.org]
`libssl` and `libcrypto` (openssl)[https://www.openssl.org]

## UNIX/LINUX - MacOS (terminal) - WINDOWS (cygwin, MinGW)


To build the library, type from source tree directory:
```
make
```
Binaries are then located in the 'lib' directory.

To install the library.
Copy the files in `lib` to `/usr/local/lib` or the equivalent for your environment.
Copy the files in `include` to `/usr/local/include` or the equivalent for your environment.

To build with `libc4.a` add `-lc4` to your build flags.

## Tests

To build the unit tests go to the `tests` folder and `make`. The run `testing` from the command line.
