; json-asm: SIMD structural character detection for x86-64
; Finds {}[]":, characters for JSON tokenization
;
; Returns a bitmask where set bits indicate structural characters

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
    ; AVX-512 constants (64-byte aligned)
    const_lbrace_64:    times 64 db '{'
    const_rbrace_64:    times 64 db '}'
    const_lbrack_64:    times 64 db '['
    const_rbrack_64:    times 64 db ']'
    const_colon_64:     times 64 db ':'
    const_comma_64:     times 64 db ','
    const_quote_64:     times 64 db '"'

    align 32
    ; AVX2 constants (32-byte aligned)
    const_lbrace_32:    times 32 db '{'
    const_rbrace_32:    times 32 db '}'
    const_lbrack_32:    times 32 db '['
    const_rbrack_32:    times 32 db ']'
    const_colon_32:     times 32 db ':'
    const_comma_32:     times 32 db ','
    const_quote_32:     times 32 db '"'

    align 16
    ; SSE constants (16-byte aligned)
    const_lbrace_16:    times 16 db '{'
    const_rbrace_16:    times 16 db '}'
    const_lbrack_16:    times 16 db '['
    const_rbrack_16:    times 16 db ']'
    const_colon_16:     times 16 db ':'
    const_comma_16:     times 16 db ','
    const_quote_16:     times 16 db '"'

section .text

; ============================================================================
; AVX-512 implementation (64 bytes at a time)
; ============================================================================
%ifndef NO_AVX512

GLOBAL_FUNC find_structural_avx512
    ; Input: rdi = string pointer, rsi = length, rdx = mask output pointer (uint64_t*)
    ; Output: rax = bytes processed (up to 64)

    test    rsi, rsi
    jz      .empty

    ; Cap at 64 bytes
    mov     rax, 64
    cmp     rsi, 64
    cmovb   rax, rsi

    ; Load structural character constants
    vmovdqa64 zmm1, [rel const_lbrace_64]
    vmovdqa64 zmm2, [rel const_rbrace_64]
    vmovdqa64 zmm3, [rel const_lbrack_64]
    vmovdqa64 zmm4, [rel const_rbrack_64]
    vmovdqa64 zmm5, [rel const_colon_64]
    vmovdqa64 zmm6, [rel const_comma_64]
    vmovdqa64 zmm7, [rel const_quote_64]

    ; Create mask for valid bytes
    mov     rcx, -1
    bzhi    rcx, rcx, rax                     ; Mask for valid bytes
    kmovq   k7, rcx

    ; Load input with mask (zeros invalid bytes)
    vmovdqu8 zmm0{k7}{z}, [rdi]

    ; Compare against each structural character and OR results
    vpcmpeqb k1, zmm0, zmm1                   ; '{'
    vpcmpeqb k2, zmm0, zmm2                   ; '}'
    korq    k1, k1, k2

    vpcmpeqb k2, zmm0, zmm3                   ; '['
    korq    k1, k1, k2

    vpcmpeqb k2, zmm0, zmm4                   ; ']'
    korq    k1, k1, k2

    vpcmpeqb k2, zmm0, zmm5                   ; ':'
    korq    k1, k1, k2

    vpcmpeqb k2, zmm0, zmm6                   ; ','
    korq    k1, k1, k2

    vpcmpeqb k2, zmm0, zmm7                   ; '"'
    korq    k1, k1, k2

    ; Store result mask
    kmovq   [rdx], k1

    vzeroupper
    ret

.empty:
    xor     eax, eax
    mov     qword [rdx], 0
    ret

%endif ; NO_AVX512

; ============================================================================
; AVX2 implementation (32 bytes at a time)
; ============================================================================

GLOBAL_FUNC find_structural_avx2
    ; Input: rdi = string pointer, rsi = length, rdx = mask output pointer (uint64_t*)
    ; Output: rax = bytes processed (up to 32)

    test    rsi, rsi
    jz      .avx2_empty

    ; Cap at 32 bytes
    mov     rax, 32
    cmp     rsi, 32
    cmovb   rax, rsi

    ; Load structural character constants
    vmovdqa ymm1, [rel const_lbrace_32]
    vmovdqa ymm2, [rel const_rbrace_32]
    vmovdqa ymm3, [rel const_lbrack_32]
    vmovdqa ymm4, [rel const_rbrack_32]
    vmovdqa ymm5, [rel const_colon_32]
    vmovdqa ymm6, [rel const_comma_32]
    vmovdqa ymm7, [rel const_quote_32]

    ; Load input
    vmovdqu ymm0, [rdi]

    ; Compare against each structural character
    vpcmpeqb ymm8, ymm0, ymm1                 ; '{'
    vpcmpeqb ymm9, ymm0, ymm2                 ; '}'
    vpor    ymm8, ymm8, ymm9

    vpcmpeqb ymm9, ymm0, ymm3                 ; '['
    vpor    ymm8, ymm8, ymm9

    vpcmpeqb ymm9, ymm0, ymm4                 ; ']'
    vpor    ymm8, ymm8, ymm9

    vpcmpeqb ymm9, ymm0, ymm5                 ; ':'
    vpor    ymm8, ymm8, ymm9

    vpcmpeqb ymm9, ymm0, ymm6                 ; ','
    vpor    ymm8, ymm8, ymm9

    vpcmpeqb ymm9, ymm0, ymm7                 ; '"'
    vpor    ymm8, ymm8, ymm9

    ; Extract mask to ecx
    vpmovmskb ecx, ymm8

    ; Create validity mask based on actual length
    ; If rax < 32, mask out bits >= rax
    mov     r8d, -1                           ; All 1s
    mov     r9, rax                           ; Copy count to r9
    mov     cl, r9b                           ; Put count in cl for shift
    shl     r8d, cl                           ; Shift left by count
    not     r8d                               ; Invert to get mask of valid bits

    ; Re-extract mask (since we clobbered ecx)
    vpmovmskb ecx, ymm8
    and     ecx, r8d                          ; Mask out invalid positions

    ; Store result (zero-extend to 64-bit)
    mov     dword [rdx], ecx
    mov     dword [rdx + 4], 0

    vzeroupper
    ret

.avx2_empty:
    xor     eax, eax
    mov     qword [rdx], 0
    ret

; ============================================================================
; SSE4.2 implementation (16 bytes at a time)
; ============================================================================

GLOBAL_FUNC find_structural_sse42
    ; Input: rdi = string pointer, rsi = length, rdx = mask output pointer (uint64_t*)
    ; Output: rax = bytes processed (up to 16)

    test    rsi, rsi
    jz      .sse_empty

    ; Cap at 16 bytes
    mov     rax, 16
    cmp     rsi, 16
    cmovb   rax, rsi

    ; Load input
    movdqu  xmm0, [rdi]

    ; Check '{'
    movdqa  xmm1, [rel const_lbrace_16]
    movdqa  xmm8, xmm0
    pcmpeqb xmm8, xmm1

    ; Check '}'
    movdqa  xmm1, [rel const_rbrace_16]
    movdqa  xmm9, xmm0
    pcmpeqb xmm9, xmm1
    por     xmm8, xmm9

    ; Check '['
    movdqa  xmm1, [rel const_lbrack_16]
    movdqa  xmm9, xmm0
    pcmpeqb xmm9, xmm1
    por     xmm8, xmm9

    ; Check ']'
    movdqa  xmm1, [rel const_rbrack_16]
    movdqa  xmm9, xmm0
    pcmpeqb xmm9, xmm1
    por     xmm8, xmm9

    ; Check ':'
    movdqa  xmm1, [rel const_colon_16]
    movdqa  xmm9, xmm0
    pcmpeqb xmm9, xmm1
    por     xmm8, xmm9

    ; Check ','
    movdqa  xmm1, [rel const_comma_16]
    movdqa  xmm9, xmm0
    pcmpeqb xmm9, xmm1
    por     xmm8, xmm9

    ; Check '"'
    movdqa  xmm1, [rel const_quote_16]
    movdqa  xmm9, xmm0
    pcmpeqb xmm9, xmm1
    por     xmm8, xmm9

    ; Extract mask
    pmovmskb ecx, xmm8

    ; Create validity mask
    mov     r8d, -1
    mov     r9, rax
    mov     cl, r9b
    shl     r8d, cl
    not     r8d

    ; Re-extract and mask
    pmovmskb ecx, xmm8
    and     ecx, r8d

    ; Store result
    mov     dword [rdx], ecx
    mov     dword [rdx + 4], 0

    ret

.sse_empty:
    xor     eax, eax
    mov     qword [rdx], 0
    ret

; Mark stack as non-executable (security)
section .note.GNU-stack noalloc noexec nowrite progbits
