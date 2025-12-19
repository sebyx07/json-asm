# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

json-asm is the fastest JSON parser/serializer, written in hand-optimized x86-64 and ARM64 assembly with a C API. Performance is the primary goal—every cycle counts.

## Build Commands

```bash
make                  # Build static and shared libraries
make test             # Build and run test suite
make bench            # Build and run benchmarks
make install          # Install to /usr/local (or PREFIX=)
make clean            # Remove build artifacts

# Debug/feature builds
make DEBUG=1          # Debug build with symbols
make ASAN=1           # AddressSanitizer
make NO_AVX512=1      # Disable AVX-512 (x86-64)
make NO_SVE=1         # Disable SVE (ARM64)

# Run single test
./build/test_parser

# Benchmark specific file
./build/bench --file data/twitter.json
```

## Architecture

### Dual-Architecture SIMD

The library has separate assembly implementations for x86-64 and ARM64, with runtime CPU feature detection:

- **x86-64**: AVX-512 → AVX2 → SSE4.2 → Scalar (NASM assembly)
- **ARM64**: SVE2 → SVE → NEON → Scalar (inline assembly via GCC/Clang)

### Value Node Layout (24 bytes)

```
Bytes 0-7:   4-bit type tag + 60-bit payload (inline small ints, short strings ≤7 bytes)
Bytes 8-15:  next sibling pointer OR string length
Bytes 16-23: first child pointer OR string pointer
```

This is 25% smaller than yyjson's 32-byte nodes.

### Hot Path Functions

All performance-critical code is in assembly with runtime dispatch via function pointers:
- `scan_string_*` - SIMD string scanning for `"`, `\`, control chars
- `find_structural_*` - SIMD detection of `{}[]":,`
- `parse_int_*` - Branchless integer parsing
- `parse_float_*` - Two-phase float parsing (integer accumulation + power-of-10 scaling)

### Parser State Machine

Table-driven with computed goto dispatch. Transition table: `[state][char_class] → (new_state, action)` packed in 8 bits.

## Performance Requirements

- All changes to hot paths must include benchmark results
- Avoid branches in parsing loops (use cmov, predication, branchless algorithms)
- Maintain 64-byte cache line alignment for constants
- Use explicit prefetching for sequential access patterns

## Testing Correctness

Run the full test suite with sanitizers before committing:
```bash
make clean && make ASAN=1 UBSAN=1 && make test
```
