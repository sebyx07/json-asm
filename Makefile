# json-asm Makefile
# Hand-optimized JSON parser/serializer for x86-64 and ARM64

# Detect architecture
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
    DETECTED_ARCH := x86-64
else ifeq ($(UNAME_M),aarch64)
    DETECTED_ARCH := arm64
else ifeq ($(UNAME_M),arm64)
    DETECTED_ARCH := arm64
else
    DETECTED_ARCH := unknown
endif

ARCH ?= $(DETECTED_ARCH)

# Detect OS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    OS := macos
    SHARED_EXT := dylib
    SHARED_FLAGS := -dynamiclib
else ifeq ($(UNAME_S),Linux)
    OS := linux
    SHARED_EXT := so
    SHARED_FLAGS := -shared -Wl,-soname,libjsonasm.so.1
else
    OS := unknown
    SHARED_EXT := so
    SHARED_FLAGS := -shared
endif

# Compilers and tools
CC ?= gcc
AR ?= ar
NASM ?= nasm

# Check if NASM is available (for x86-64)
HAVE_NASM := $(shell which $(NASM) 2>/dev/null)
ifeq ($(HAVE_NASM),)
    USE_ASM := 0
else
    USE_ASM := 1
endif

# Installation paths
PREFIX ?= /usr/local
LIBDIR := $(PREFIX)/lib
INCLUDEDIR := $(PREFIX)/include
PKGCONFIGDIR := $(LIBDIR)/pkgconfig

# Build directory
BUILD := build

# Compiler flags
CFLAGS := -O3 -Wall -Wextra -Wpedantic -std=c11
CFLAGS += -fPIC -I include
CFLAGS += -fvisibility=hidden

# Architecture-specific flags
ifeq ($(ARCH),x86-64)
    CFLAGS += -DJSON_ASM_X86_64
    MARCH ?= native
    ifneq ($(MARCH),native)
        CFLAGS += -march=$(MARCH)
    else
        CFLAGS += -march=native
    endif
else ifeq ($(ARCH),arm64)
    CFLAGS += -DJSON_ASM_ARM64
    MARCH ?= native
    ifeq ($(OS),macos)
        # Apple Silicon
        CFLAGS += -mcpu=apple-m1
    else ifneq ($(MARCH),native)
        CFLAGS += -mcpu=$(MARCH)
    endif
endif

# NASM flags for x86-64
ifeq ($(OS),macos)
    NASM_FORMAT := macho64
    NASM_FLAGS := -f $(NASM_FORMAT) -DMACHO
else
    NASM_FORMAT := elf64
    NASM_FLAGS := -f $(NASM_FORMAT) -DELF
endif

# Feature disable flags
ifdef NO_AVX512
    CFLAGS += -DNO_AVX512
    NASM_FLAGS += -DNO_AVX512
endif

ifdef NO_SVE
    CFLAGS += -DNO_SVE
endif

ifdef MAX_SIMD
    CFLAGS += -DMAX_SIMD_$(MAX_SIMD)
    NASM_FLAGS += -DMAX_SIMD_$(MAX_SIMD)
endif

# Debug build
ifdef DEBUG
    CFLAGS := -O0 -g3 -Wall -Wextra -Wpedantic -std=c11
    CFLAGS += -fPIC -I include -DDEBUG
    ifeq ($(ARCH),x86-64)
        CFLAGS += -DJSON_ASM_X86_64
    else ifeq ($(ARCH),arm64)
        CFLAGS += -DJSON_ASM_ARM64
    endif
    NASM_FLAGS += -g -DDEBUG
endif

# Sanitizers
ifdef ASAN
    CFLAGS += -fsanitize=address -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=address
endif

ifdef UBSAN
    CFLAGS += -fsanitize=undefined -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=undefined
endif

ifdef TSAN
    CFLAGS += -fsanitize=thread -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=thread
endif

# Source files
C_SRCS := src/json_asm.c \
          src/cpu_detect.c \
          src/arena.c \
          src/parse.c \
          src/stringify.c

# Architecture-specific sources
ifeq ($(ARCH),x86-64)
    ifeq ($(USE_ASM),1)
        ASM_SRCS := src/x86-64/scan_string.asm \
                    src/x86-64/find_structural.asm \
                    src/x86-64/parse_number.asm
        ASM_OBJS := $(patsubst src/x86-64/%.asm,$(BUILD)/%.o,$(ASM_SRCS))
    else
        # No NASM available - use scalar fallback
        CFLAGS += -DUSE_SCALAR_ONLY
        ASM_OBJS :=
    endif
else ifeq ($(ARCH),arm64)
    C_SRCS += src/arm64/simd_arm64.c
    ASM_OBJS :=
else
    # Unknown architecture - use scalar only
    CFLAGS += -DUSE_SCALAR_ONLY
    ASM_OBJS :=
endif

C_OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(C_SRCS))
OBJS := $(C_OBJS) $(ASM_OBJS)

# Library names
STATIC_LIB := $(BUILD)/libjsonasm.a
SHARED_LIB := $(BUILD)/libjsonasm.$(SHARED_EXT)

# Test and benchmark sources
TEST_SRCS := tests/test_parser.c tests/test_stringify.c tests/test_api.c
TEST_BINS := $(patsubst tests/%.c,$(BUILD)/%,$(TEST_SRCS))

BENCH_SRC := bench/bench.c
BENCH_BIN := $(BUILD)/bench

DETECT_BIN := $(BUILD)/detect_features

# Default target
.PHONY: all
all: $(STATIC_LIB) $(SHARED_LIB)

# Static library
.PHONY: static
static: $(STATIC_LIB)

$(STATIC_LIB): $(OBJS) | $(BUILD)
	$(AR) rcs $@ $^

# Shared library
.PHONY: shared
shared: $(SHARED_LIB)

$(SHARED_LIB): $(OBJS) | $(BUILD)
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $^

# Object compilation - C files
$(BUILD)/%.o: src/%.c | $(BUILD) $(BUILD)/arm64
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/arm64/%.o: src/arm64/%.c | $(BUILD)/arm64
	$(CC) $(CFLAGS) -c -o $@ $<

# Object compilation - x86-64 NASM assembly
$(BUILD)/%.o: src/x86-64/%.asm | $(BUILD)
	$(NASM) $(NASM_FLAGS) -o $@ $<

# Create build directories
$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/arm64:
	mkdir -p $(BUILD)/arm64

# Tests
.PHONY: test
test: $(TEST_BINS)
	@echo "Running tests..."
	@for t in $(TEST_BINS); do \
		echo "  $$t"; \
		$$t || exit 1; \
	done
	@echo "All tests passed!"

$(BUILD)/test_%: tests/test_%.c $(STATIC_LIB) | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(STATIC_LIB) -lm

# Benchmark
.PHONY: bench
bench: $(BENCH_BIN)
	$(BENCH_BIN)

$(BENCH_BIN): $(BENCH_SRC) $(STATIC_LIB) | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(STATIC_LIB) -lm

# CPU feature detection tool
.PHONY: detect
detect: $(DETECT_BIN)
	$(DETECT_BIN)

$(DETECT_BIN): src/cpu_detect.c | $(BUILD)
	$(CC) $(CFLAGS) -DDETECT_MAIN -o $@ $<

# Installation
.PHONY: install
install: all
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -d $(DESTDIR)$(PKGCONFIGDIR)
	install -m 644 $(STATIC_LIB) $(DESTDIR)$(LIBDIR)/
	install -m 755 $(SHARED_LIB) $(DESTDIR)$(LIBDIR)/
	install -m 644 include/json_asm.h $(DESTDIR)$(INCLUDEDIR)/
	@echo "Generating pkg-config file..."
	@echo 'prefix=$(PREFIX)' > $(DESTDIR)$(PKGCONFIGDIR)/jsonasm.pc
	@echo 'libdir=$${prefix}/lib' >> $(DESTDIR)$(PKGCONFIGDIR)/jsonasm.pc
	@echo 'includedir=$${prefix}/include' >> $(DESTDIR)$(PKGCONFIGDIR)/jsonasm.pc
	@echo '' >> $(DESTDIR)$(PKGCONFIGDIR)/jsonasm.pc
	@echo 'Name: jsonasm' >> $(DESTDIR)$(PKGCONFIGDIR)/jsonasm.pc
	@echo 'Description: Fast JSON parser/serializer in hand-optimized assembly' >> $(DESTDIR)$(PKGCONFIGDIR)/jsonasm.pc
	@echo 'Version: 1.0.0' >> $(DESTDIR)$(PKGCONFIGDIR)/jsonasm.pc
	@echo 'Libs: -L$${libdir} -ljsonasm' >> $(DESTDIR)$(PKGCONFIGDIR)/jsonasm.pc
	@echo 'Cflags: -I$${includedir}' >> $(DESTDIR)$(PKGCONFIGDIR)/jsonasm.pc
ifeq ($(OS),linux)
	ldconfig $(DESTDIR)$(LIBDIR) 2>/dev/null || true
endif
	@echo "Installed to $(PREFIX)"

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/libjsonasm.a
	rm -f $(DESTDIR)$(LIBDIR)/libjsonasm.$(SHARED_EXT)
	rm -f $(DESTDIR)$(INCLUDEDIR)/json_asm.h
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/jsonasm.pc

# Clean
.PHONY: clean
clean:
	rm -rf $(BUILD)

# Verbose mode
ifdef VERBOSE
    Q :=
else
    Q := @
endif

# Help
.PHONY: help
help:
	@echo "json-asm build system"
	@echo ""
	@echo "Targets:"
	@echo "  all          Build static and shared libraries (default)"
	@echo "  static       Build static library only"
	@echo "  shared       Build shared library only"
	@echo "  test         Build and run tests"
	@echo "  bench        Build and run benchmarks"
	@echo "  detect       Build and run CPU feature detection"
	@echo "  install      Install to PREFIX (default: /usr/local)"
	@echo "  uninstall    Remove installed files"
	@echo "  clean        Remove build artifacts"
	@echo ""
	@echo "Options:"
	@echo "  CC=...       C compiler (default: gcc)"
	@echo "  PREFIX=...   Installation prefix (default: /usr/local)"
	@echo "  DEBUG=1      Debug build with symbols"
	@echo "  ASAN=1       Enable AddressSanitizer"
	@echo "  UBSAN=1      Enable UndefinedBehaviorSanitizer"
	@echo "  TSAN=1       Enable ThreadSanitizer"
	@echo "  NO_AVX512=1  Disable AVX-512 code (x86-64)"
	@echo "  NO_SVE=1     Disable SVE code (ARM64)"
	@echo "  MARCH=...    Target microarchitecture"
	@echo "  VERBOSE=1    Show build commands"
	@echo ""
	@echo "Detected: ARCH=$(ARCH), OS=$(OS)"
