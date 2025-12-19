; json-asm: SIMD string scanning for x86-64
; Scans for quote, backslash, or control characters (< 0x20)
;
; These functions scan a string looking for special characters that need
; handling during JSON string parsing: ", \, and control chars (0x00-0x1F)

%ifdef MACHO
    %define FUNC(name) _ %+ name
%else
    %define FUNC(name) name
%endif

%macro GLOBAL_FUNC 1
    global FUNC(%1):function
    FUNC(%1):
%endmacro

; ============================================================================
; Read-only data section
; ============================================================================
section .rodata
    align 64
    ; Constants for AVX-512 (64-byte aligned)
    const_quote_64:     times 64 db '"'
    const_bslash_64:    times 64 db '\'
    const_ctrl_max_64:  times 64 db 0x1F    ; Max control char value

    align 32
    ; Constants for AVX2 (32-byte aligned)
    const_quote_32:     times 32 db '"'
    const_bslash_32:    times 32 db '\'
    const_ctrl_max_32:  times 32 db 0x1F

    align 16
    ; Constants for SSE (16-byte aligned)
    const_quote_16:     times 16 db '"'
    const_bslash_16:    times 16 db '\'
    const_ctrl_max_16:  times 16 db 0x1F

section .text

; ============================================================================
; AVX-512 implementation (64 bytes at a time)
; ============================================================================
%ifndef NO_AVX512

GLOBAL_FUNC scan_string_avx512
    ; Input: rdi = string pointer, rsi = length
    ; Output: rax = position of special char, or length if none found

    test    rsi, rsi
    jz      .empty

    ; Load broadcast constants
    vmovdqa64 zmm1, [rel const_quote_64]      ; '"'
    vmovdqa64 zmm2, [rel const_bslash_64]     ; '\'
    vmovdqa64 zmm3, [rel const_ctrl_max_64]   ; 0x1F

    xor     rax, rax                           ; Position counter

.loop64:
    mov     rcx, rsi
    sub     rcx, rax
    cmp     rcx, 64
    jb      .tail

    ; Load 64 bytes
    vmovdqu64 zmm0, [rdi + rax]

    ; Check for '"' (quote)
    vpcmpeqb k1, zmm0, zmm1

    ; Check for '\' (backslash)
    vpcmpeqb k2, zmm0, zmm2
    korq    k1, k1, k2

    ; Check for control chars (byte <= 0x1F)
    ; Use unsigned min: if min(byte, 0x1F) == byte, then byte <= 0x1F
    vpminub zmm4, zmm0, zmm3
    vpcmpeqb k2, zmm4, zmm0
    korq    k1, k1, k2

    ; Check if any match found
    kmovq   rcx, k1
    test    rcx, rcx
    jnz     .found64

    add     rax, 64
    jmp     .loop64

.found64:
    tzcnt   rcx, rcx                          ; Find first set bit
    add     rax, rcx
    vzeroupper
    ret

.tail:
    ; Handle remaining bytes with scalar
    jmp     .scalar_loop

.empty:
    xor     eax, eax
    ret

.scalar_loop:
    cmp     rax, rsi
    jae     .done
    movzx   ecx, byte [rdi + rax]
    cmp     cl, '"'
    je      .done
    cmp     cl, '\'
    je      .done
    cmp     cl, 0x20
    jb      .done
    inc     rax
    jmp     .scalar_loop

.done:
    vzeroupper
    ret

%endif ; NO_AVX512

; ============================================================================
; AVX2 implementation (32 bytes at a time)
; ============================================================================

GLOBAL_FUNC scan_string_avx2
    ; Input: rdi = string pointer, rsi = length
    ; Output: rax = position of special char, or length if none found

    test    rsi, rsi
    jz      .avx2_empty

    ; Load constants
    vmovdqa ymm1, [rel const_quote_32]
    vmovdqa ymm2, [rel const_bslash_32]
    vmovdqa ymm3, [rel const_ctrl_max_32]

    xor     rax, rax

.avx2_loop:
    mov     rcx, rsi
    sub     rcx, rax
    cmp     rcx, 32
    jb      .avx2_tail

    ; Load 32 bytes
    vmovdqu ymm0, [rdi + rax]

    ; Check for '"' (quote)
    vpcmpeqb ymm4, ymm0, ymm1

    ; Check for '\' (backslash)
    vpcmpeqb ymm5, ymm0, ymm2
    vpor    ymm4, ymm4, ymm5

    ; Check for control chars (byte <= 0x1F)
    ; vpminub gives min(byte, 0x1F), compare equal to original means byte <= 0x1F
    vpminub ymm5, ymm0, ymm3
    vpcmpeqb ymm5, ymm5, ymm0
    vpor    ymm4, ymm4, ymm5

    ; Extract mask
    vpmovmskb ecx, ymm4
    test    ecx, ecx
    jnz     .avx2_found

    add     rax, 32
    jmp     .avx2_loop

.avx2_found:
    tzcnt   ecx, ecx
    add     rax, rcx
    vzeroupper
    ret

.avx2_tail:
    cmp     rax, rsi
    jae     .avx2_done

.avx2_scalar:
    movzx   ecx, byte [rdi + rax]
    cmp     cl, '"'
    je      .avx2_done
    cmp     cl, '\'
    je      .avx2_done
    cmp     cl, 0x20
    jb      .avx2_done
    inc     rax
    cmp     rax, rsi
    jb      .avx2_scalar

.avx2_done:
    vzeroupper
    ret

.avx2_empty:
    xor     eax, eax
    ret

; ============================================================================
; SSE4.2 implementation (16 bytes at a time)
; ============================================================================

GLOBAL_FUNC scan_string_sse42
    ; Input: rdi = string pointer, rsi = length
    ; Output: rax = position of special char, or length if none found

    test    rsi, rsi
    jz      .sse_empty

    ; Load constants
    movdqa  xmm1, [rel const_quote_16]
    movdqa  xmm2, [rel const_bslash_16]
    movdqa  xmm3, [rel const_ctrl_max_16]

    xor     rax, rax

.sse_loop:
    mov     rcx, rsi
    sub     rcx, rax
    cmp     rcx, 16
    jb      .sse_tail

    ; Load 16 bytes
    movdqu  xmm0, [rdi + rax]

    ; Check for '"'
    movdqa  xmm4, xmm0
    pcmpeqb xmm4, xmm1

    ; Check for '\'
    movdqa  xmm5, xmm0
    pcmpeqb xmm5, xmm2
    por     xmm4, xmm5

    ; Check for control chars <= 0x1F
    movdqa  xmm5, xmm0
    pminub  xmm5, xmm3              ; min(byte, 0x1F)
    pcmpeqb xmm5, xmm0              ; true if byte <= 0x1F
    por     xmm4, xmm5

    ; Extract mask
    pmovmskb ecx, xmm4
    test    ecx, ecx
    jnz     .sse_found

    add     rax, 16
    jmp     .sse_loop

.sse_found:
    tzcnt   ecx, ecx
    add     rax, rcx
    ret

.sse_tail:
    cmp     rax, rsi
    jae     .sse_done

.sse_scalar:
    movzx   ecx, byte [rdi + rax]
    cmp     cl, '"'
    je      .sse_done
    cmp     cl, '\'
    je      .sse_done
    cmp     cl, 0x20
    jb      .sse_done
    inc     rax
    cmp     rax, rsi
    jb      .sse_scalar

.sse_done:
    ret

.sse_empty:
    xor     eax, eax
    ret

; Mark stack as non-executable (security)
section .note.GNU-stack noalloc noexec nowrite progbits
