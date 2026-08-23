# Apple Silicon

This example targets macOS on Apple Silicon. It emits AArch64 assembly and
Mach-O objects, and links against the macOS C runtime.

## Layout

- `src/` contains the C sources and headers.
- `build/clang/` contains the native Apple Clang artifacts.
- `build/gcc/` contains artifacts produced through the `gcc` alias.

## Build pipeline

Run these commands from this directory:

```sh
mkdir -p build/clang
clang -E src/main.c -Isrc -o build/clang/main.i
clang -E src/square.c -Isrc -o build/clang/square.i
clang -S src/main.c -Isrc -o build/clang/main.s
clang -S src/square.c -Isrc -o build/clang/square.s
clang -c src/main.c -Isrc -o build/clang/main.o
clang -c src/square.c -Isrc -o build/clang/square.o
clang build/clang/main.o build/clang/square.o -o build/clang/square
./build/clang/square
```

Expected output:

```text
The square of 10 is 100
```

`clang` and Apple's `gcc` command are aliases for the native Apple Clang
toolchain. Optimization levels will be compared in a later assembly study;
the commands above intentionally use the compiler default.
