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

## Optimization snapshot

The `build/clang/` output is the unoptimized baseline (`-O0`). To generate an
optimized snapshot without replacing it:

```sh
mkdir -p build/o1
clang -O1 -S src/main.c -Isrc -o build/o1/main.s
clang -O1 -S src/square.c -Isrc -o build/o1/square.s
clang -O1 -c src/main.c -Isrc -o build/o1/main.o
clang -O1 -c src/square.c -Isrc -o build/o1/square.o
clang build/o1/main.o build/o1/square.o -o build/o1/square
./build/o1/square
```

The same commands with `-O2` are stored in `build/o2/` for comparison:

```sh
mkdir -p build/o2
clang -O2 -S src/main.c -Isrc -o build/o2/main.s
clang -O2 -S src/square.c -Isrc -o build/o2/square.s
clang -O2 -c src/main.c -Isrc -o build/o2/main.o
clang -O2 -c src/square.c -Isrc -o build/o2/square.o
clang build/o2/main.o build/o2/square.o -o build/o2/square
./build/o2/square
```
