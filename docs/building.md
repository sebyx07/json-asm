# Building

Build instructions for x86-64 and ARM64 platforms.

## Table of Contents

- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Build Options](#build-options)
- [x86-64 Build](#x86-64-build)
- [ARM64 Build](#arm64-build)
- [Cross-Compilation](#cross-compilation)
- [Troubleshooting](#troubleshooting)
- [Package Managers](#package-managers)

---

## Requirements

### x86-64

| Tool    | Version  | Purpose                    |
|---------|----------|----------------------------|
| NASM    | 2.15+    | x86-64 assembly            |
| GCC     | 10+      | C compilation and linking  |
| Make    | 4.0+     | Build orchestration        |

### ARM64

| Tool    | Version  | Purpose                    |
|---------|----------|----------------------------|
| GCC     | 10+      | C/inline assembly          |
| Make    | 4.0+     | Build orchestration        |

### Alternative Compilers

| Compiler | x86-64 | ARM64 | Notes                    |
|----------|--------|-------|--------------------------|
| GCC      | 10+    | 10+   | Full support             |
| Clang    | 12+    | 12+   | Full support             |
| ICC      | 2021+  | N/A   | x86-64 only              |
| MSVC     | 2022+  | N/A   | x86-64 Windows only      |

### CPU Requirements

**x86-64:**
- Minimum: SSE4.2, POPCNT
- Recommended: AVX2, BMI2
- Optimal: AVX-512 (F, BW, VL)

**ARM64:**
- Minimum: ARMv8-A (NEON always present)
- Recommended: ARMv8.2-A
- Optimal: ARMv9-A (SVE2)

---

## Quick Start

### x86-64 (Linux)

```bash
# Install dependencies
sudo apt install nasm gcc make    # Debian/Ubuntu
sudo dnf install nasm gcc make    # Fedora
sudo pacman -S nasm gcc make      # Arch

# Build
git clone https://github.com/example/json-asm.git
cd json-asm
make
make test
sudo make install
```

### ARM64 (Linux)

```bash
# Install dependencies
sudo apt install gcc make         # Debian/Ubuntu

# Build
git clone https://github.com/example/json-asm.git
cd json-asm
make
make test
sudo make install
```

### macOS (Apple Silicon or Intel)

```bash
# Install Xcode command line tools
xcode-select --install

# For Intel Macs, install NASM
brew install nasm

# Build
make
make test
sudo make install
```

---

## Build Options

### Make Variables

```bash
# Compiler selection
make CC=clang

# Installation prefix
make PREFIX=/opt/json-asm install

# Debug build
make DEBUG=1

# Architecture selection
make ARCH=x86-64          # Force x86-64 build
make ARCH=arm64           # Force ARM64 build

# CPU feature limits
make NO_AVX512=1          # Disable AVX-512 code (x86-64)
make NO_SVE=1             # Disable SVE code (ARM64)
make MAX_SIMD=avx2        # Cap at AVX2 (x86-64)
make MAX_SIMD=neon        # Cap at NEON (ARM64)

# Library type
make STATIC_ONLY=1        # Only build static library
make SHARED_ONLY=1        # Only build shared library

# Sanitizers
make ASAN=1               # AddressSanitizer
make UBSAN=1              # UndefinedBehaviorSanitizer
make TSAN=1               # ThreadSanitizer
```

### Build Targets

| Target           | Description                               |
|------------------|-------------------------------------------|
| `make`           | Build all libraries                       |
| `make static`    | Build static library only                 |
| `make shared`    | Build shared library only                 |
| `make test`      | Build and run tests                       |
| `make bench`     | Build and run benchmarks                  |
| `make clean`     | Remove build artifacts                    |
| `make install`   | Install to PREFIX                         |
| `make uninstall` | Remove installed files                    |

---

## x86-64 Build

### Linux (Debian/Ubuntu)

```bash
# Install dependencies
sudo apt update
sudo apt install nasm gcc make

# Build with all optimizations
make

# Check detected CPU features
./build/detect_features
# Output: AVX512F AVX512BW AVX512VL AVX2 BMI2 POPCNT SSE42

# Install
sudo make install
sudo ldconfig
```

### Linux (RHEL/CentOS/Rocky)

```bash
# Install EPEL for NASM
sudo dnf install epel-release
sudo dnf install nasm gcc make

make
sudo make install
```

### macOS (Intel)

```bash
# Install NASM (Apple's as doesn't support AVX-512)
brew install nasm

# Build (auto-detects format=macho64)
make

# Install
sudo make install
```

### Windows (MSYS2/MinGW-w64)

```bash
# In MSYS2 MinGW64 shell
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-nasm make

make

# Output in build/
ls build/
# jsonasm.dll jsonasm.lib libjsonasm.a
```

### Windows (Visual Studio)

```batch
:: Open x64 Native Tools Command Prompt

:: Ensure NASM is in PATH
where nasm

:: Build
nmake /f Makefile.msvc

:: Install
nmake /f Makefile.msvc install PREFIX=C:\json-asm
```

### Feature-Specific Builds

```bash
# Build optimized for specific microarchitecture
make MARCH=skylake-avx512     # Intel Skylake-X
make MARCH=znver4             # AMD Zen 4
make MARCH=alderlake          # Intel Alder Lake

# Build portable binary with runtime dispatch
make MARCH=x86-64-v2          # SSE4.2 baseline
make MARCH=x86-64-v3          # AVX2 baseline
make MARCH=x86-64-v4          # AVX-512 baseline
```

---

## ARM64 Build

### Linux (Debian/Ubuntu on ARM64)

```bash
# Install dependencies
sudo apt update
sudo apt install gcc make

# Build
make

# Check detected features
./build/detect_features
# Output: NEON SVE SVE2 DOTPROD (varies by CPU)

# Install
sudo make install
sudo ldconfig
```

### Linux (AWS Graviton)

```bash
# Amazon Linux 2023
sudo dnf install gcc make

# Build with Graviton optimizations
make MARCH=neoverse-v1        # Graviton 3
make MARCH=neoverse-n1        # Graviton 2

make
sudo make install
```

### macOS (Apple Silicon)

```bash
# Install Xcode tools
xcode-select --install

# Build (auto-detects ARM64 and uses clang)
make

# Install
sudo make install
```

### Android (NDK)

```bash
# Set NDK path
export NDK=/path/to/android-ndk

# Build for ARM64
make CC=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android33-clang \
     AR=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar \
     ARCH=arm64

# Output: build/libjsonasm.a build/libjsonasm.so
```

### iOS

```bash
# Build for iOS ARM64
make CC="xcrun -sdk iphoneos clang" \
     CFLAGS="-arch arm64 -mios-version-min=13.0" \
     ARCH=arm64 \
     STATIC_ONLY=1
```

### Feature-Specific ARM64 Builds

```bash
# Target specific ARM cores
make MARCH=neoverse-v1        # AWS Graviton 3, Ampere Altra
make MARCH=neoverse-n2        # Graviton 3E
make MARCH=cortex-a78         # Newer mobile cores
make MARCH=apple-m1           # Apple M1 (macOS only)

# Disable SVE for NEON-only binary
make NO_SVE=1
```

---

## Cross-Compilation

### x86-64 to ARM64

```bash
# Install cross-compiler
sudo apt install gcc-aarch64-linux-gnu

# Build
make CC=aarch64-linux-gnu-gcc \
     AR=aarch64-linux-gnu-ar \
     ARCH=arm64

# Test with QEMU
sudo apt install qemu-user
qemu-aarch64 -L /usr/aarch64-linux-gnu ./build/test_parser
```

### ARM64 to x86-64

```bash
# Install cross-compiler and NASM
sudo apt install gcc-x86-64-linux-gnu nasm

# Build
make CC=x86_64-linux-gnu-gcc \
     AR=x86_64-linux-gnu-ar \
     ARCH=x86-64

# Test with QEMU
sudo apt install qemu-user
qemu-x86_64 ./build/test_parser
```

### Building Universal Binary (macOS)

```bash
# Build for both architectures
make ARCH=x86-64 PREFIX=build/x86-64
make clean
make ARCH=arm64 PREFIX=build/arm64

# Create universal binary
lipo -create \
     build/x86-64/lib/libjsonasm.a \
     build/arm64/lib/libjsonasm.a \
     -output build/libjsonasm.a
```

---

## Troubleshooting

### NASM Not Found (x86-64)

```
Error: nasm not found
```

```bash
# Install NASM
sudo apt install nasm              # Debian/Ubuntu
brew install nasm                  # macOS
pacman -S mingw-w64-x86_64-nasm    # MSYS2

# Or specify path
make NASM=/custom/path/nasm
```

### AVX-512 Assembly Errors

```
Error: no instruction for this cpu level
```

```bash
# Update NASM (need 2.13+ for AVX-512)
nasm --version

# Or disable AVX-512
make NO_AVX512=1
```

### SVE Assembly Errors (ARM64)

```
Error: selected processor does not support 'sve'
```

```bash
# Ensure recent GCC/Clang
gcc --version   # Need 10+
clang --version # Need 12+

# Or disable SVE
make NO_SVE=1
```

### Illegal Instruction at Runtime

```
Illegal instruction (core dumped)
```

```bash
# Your CPU doesn't support the detected features
# Force lower feature level:

# x86-64
make clean && make MAX_SIMD=avx2    # If no AVX-512
make clean && make MAX_SIMD=sse42   # If no AVX2

# ARM64
make clean && make NO_SVE=1         # If no SVE
```

### Linker Errors on macOS

```
ld: symbol(s) not found for architecture arm64
```

```bash
# Ensure consistent architecture
make clean
make ARCH=arm64    # Or x86-64 for Intel
```

### Test Failures

```bash
# Run with verbose output
make test VERBOSE=1

# Run specific test
./build/test_parser

# Run with sanitizers
make clean
make ASAN=1 UBSAN=1
make test
```

---

## Package Managers

### pkg-config

After installation:

```bash
pkg-config --cflags jsonasm    # -I/usr/local/include
pkg-config --libs jsonasm      # -L/usr/local/lib -ljsonasm
```

### CMake

```cmake
find_package(JsonAsm REQUIRED)
target_link_libraries(myapp PRIVATE JsonAsm::jsonasm)
```

### vcpkg

```bash
vcpkg install json-asm
```

### Conan

```
[requires]
json-asm/1.0.0
```

---

## Output Files

```
build/
├── libjsonasm.a          # Static library
├── libjsonasm.so         # Shared library (Linux)
├── libjsonasm.dylib      # Shared library (macOS)
├── jsonasm.dll           # Shared library (Windows)
├── jsonasm.lib           # Import library (Windows)
├── test_*                # Test executables
├── bench                 # Benchmark tool
└── detect_features       # CPU feature detection

include/
└── json_asm.h            # Public header
```
