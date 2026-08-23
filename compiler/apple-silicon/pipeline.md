# C Compilation Pipeline

This example uses `src/main.c`, `src/square.c`, and the declaration in
`src/square.h` to show the stages on macOS. The existing
`compiler/x86_64-windows/` files
are a Windows MSYS2/GCC transcript and use PE/COFF; this directory uses
Mach-O output.

## Source layout

- `src/square.h` is the interface: it declares `square`.
- `src/square.c` is the implementation: it defines `square`.
- `src/main.c` includes the interface and calls the function.

The header is not compiled or linked by itself. `#include "square.h"` causes
the preprocessor to copy its declaration into each source file that includes
it. Splitting the function is useful here because `main.o` and `square.o`
must later be resolved by the linker.

## Stages

Run these commands from this directory:

```sh
mkdir -p build/clang build/gcc

# 1. Preprocess: expand headers and macros; output is still C.
clang -E src/main.c -Isrc -o build/clang/main.i
clang -E src/square.c -Isrc -o build/clang/square.i

# 2. Compile: translate preprocessed C into target assembly.
clang -S src/main.c -Isrc -o build/clang/main.s
clang -S src/square.c -Isrc -o build/clang/square.s

# 3. Assemble: translate assembly into relocatable object files.
clang -c src/main.c -Isrc -o build/clang/main.o
clang -c src/square.c -Isrc -o build/clang/square.o

# 4. Link: combine objects and the system C library into an executable.
clang build/clang/main.o build/clang/square.o -o build/clang/square

# Execute the linked program.
./build/clang/square
```

Expected output:

```text
The square of 10 is 100
```

`clang file.c -o program` performs all four stages, but the explicit commands
leave each intermediate artifact visible. `clang -v` shows the underlying
assembler and linker commands used by the driver.

## Inspecting each artifact

```sh
file build/clang/main.i build/clang/main.s build/clang/main.o build/clang/square
nm -gU build/clang/main.o build/clang/square.o build/clang/square
otool -tvV build/clang/square.o
otool -L build/clang/square
```

The important symbol transition is:

- `main.o` has an undefined reference to `square`.
- `square.o` defines `square`.
- The linker resolves that reference and assigns final addresses.

Headers provide declarations to the compiler; they do not provide a separate
linkable implementation. The implementation must be present in an object
file or library passed to the linker.

## Apple Clang via `gcc`

On this Mac, `gcc` is Apple Clang aliased as GCC. The same commands work with
either `clang` or `gcc` for the native target:

```sh
gcc -E src/main.c -Isrc -o build/gcc/main.i
gcc -S src/main.c -Isrc -o build/gcc/main.s
gcc -c src/main.c -Isrc -o build/gcc/main.o
gcc build/gcc/main.o build/clang/square.o -o build/gcc/square
```
