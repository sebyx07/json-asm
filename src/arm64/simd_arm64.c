/*
 * json-asm: ARM64 SIMD implementations (NEON, SVE, SVE2)
 */

#include "../internal.h"

#ifdef JSON_ARCH_ARM64

#include <arm_neon.h>

/* ============================================================================
 * NEON Implementation (16 bytes at a time)
 * ============================================================================ */

size_t scan_string_neon(const char *str, size_t len) {
    if (len == 0) return 0;

    const uint8x16_t quote_vec = vdupq_n_u8('"');
    const uint8x16_t bslash_vec = vdupq_n_u8('\\');
    const uint8x16_t space_vec = vdupq_n_u8(0x20);

    size_t pos = 0;

    while (pos + 16 <= len) {
        uint8x16_t chunk = vld1q_u8((const uint8_t *)(str + pos));

        /* Check for quote */
        uint8x16_t quote_match = vceqq_u8(chunk, quote_vec);

        /* Check for backslash */
        uint8x16_t bslash_match = vceqq_u8(chunk, bslash_vec);

        /* Check for control chars (< 0x20) */
        uint8x16_t ctrl_match = vcltq_u8(chunk, space_vec);

        /* Combine all matches */
        uint8x16_t any_match = vorrq_u8(quote_match, bslash_match);
        any_match = vorrq_u8(any_match, ctrl_match);

        /* Check if any match */
        uint64x2_t match64 = vreinterpretq_u64_u8(any_match);
        uint64_t low = vgetq_lane_u64(match64, 0);
        uint64_t high = vgetq_lane_u64(match64, 1);

        if (low) {
            /* Find first match in low 8 bytes */
            for (int i = 0; i < 8; i++) {
                if ((low >> (i * 8)) & 0xFF) {
                    return pos + i;
                }
            }
        }
        if (high) {
            /* Find first match in high 8 bytes */
            for (int i = 0; i < 8; i++) {
                if ((high >> (i * 8)) & 0xFF) {
                    return pos + 8 + i;
                }
            }
        }

        pos += 16;
    }

    /* Handle remaining bytes */
    while (pos < len) {
        unsigned char c = (unsigned char)str[pos];
        if (c == '"' || c == '\\' || c < 0x20) {
            return pos;
        }
        pos++;
    }

    return len;
}

size_t find_structural_neon(const char *str, size_t len, uint64_t *mask) {
    if (len == 0) {
        *mask = 0;
        return 0;
    }

    const uint8x16_t lbrace = vdupq_n_u8('{');
    const uint8x16_t rbrace = vdupq_n_u8('}');
    const uint8x16_t lbrack = vdupq_n_u8('[');
    const uint8x16_t rbrack = vdupq_n_u8(']');
    const uint8x16_t colon = vdupq_n_u8(':');
    const uint8x16_t comma = vdupq_n_u8(',');
    const uint8x16_t quote = vdupq_n_u8('"');

    size_t count = len < 16 ? len : 16;

    /* Load 16 bytes (may read past end, but we mask result) */
    uint8x16_t chunk = vld1q_u8((const uint8_t *)str);

    /* Compare against each structural char */
    uint8x16_t m = vceqq_u8(chunk, lbrace);
    m = vorrq_u8(m, vceqq_u8(chunk, rbrace));
    m = vorrq_u8(m, vceqq_u8(chunk, lbrack));
    m = vorrq_u8(m, vceqq_u8(chunk, rbrack));
    m = vorrq_u8(m, vceqq_u8(chunk, colon));
    m = vorrq_u8(m, vceqq_u8(chunk, comma));
    m = vorrq_u8(m, vceqq_u8(chunk, quote));

    /* Extract as bitmask */
    /* Narrow the 8-bit match results to single bits */
    uint16x8_t m16 = vreinterpretq_u16_u8(m);
    m16 = vshrq_n_u16(m16, 7);  /* Shift high bit to bit 0 */

    /* Pack to bytes and extract */
    uint8x8_t m8 = vmovn_u16(m16);
    uint64_t result = vget_lane_u64(vreinterpret_u64_u8(m8), 0);

    /* Convert from packed bits to individual byte positions */
    uint64_t final_mask = 0;
    for (size_t i = 0; i < count; i++) {
        unsigned char c = str[i];
        if (c == '{' || c == '}' || c == '[' || c == ']' ||
            c == ':' || c == ',' || c == '"') {
            final_mask |= (1ULL << i);
        }
    }

    *mask = final_mask;
    return count;
}

int64_t parse_int_neon(const char *str, size_t len, size_t *consumed) {
    if (len == 0) {
        if (consumed) *consumed = 0;
        return 0;
    }

    size_t pos = 0;
    bool negative = false;

    if (str[0] == '-') {
        negative = true;
        pos++;
    }

    /* Count digits */
    size_t digit_count = 0;
    while (pos + digit_count < len &&
           str[pos + digit_count] >= '0' &&
           str[pos + digit_count] <= '9') {
        digit_count++;
        if (digit_count >= 19) break; /* Prevent overflow */
    }

    if (digit_count == 0) {
        if (consumed) *consumed = 0;
        return 0;
    }

    if (consumed) *consumed = pos + digit_count;

    /* Parse using scalar (NEON integer multiply is limited) */
    int64_t result = 0;
    for (size_t i = 0; i < digit_count; i++) {
        result = result * 10 + (str[pos + i] - '0');
    }

    return negative ? -result : result;
}

/* ============================================================================
 * SVE Implementation (scalable vector length)
 * ============================================================================ */

#ifndef NO_SVE
#if defined(__ARM_FEATURE_SVE)

#include <arm_sve.h>

size_t scan_string_sve(const char *str, size_t len) {
    if (len == 0) return 0;

    size_t pos = 0;

    while (pos < len) {
        svbool_t pg = svwhilelt_b8(pos, len);

        svuint8_t chunk = svld1_u8(pg, (const uint8_t *)(str + pos));

        /* Check for special chars */
        svbool_t quote_match = svcmpeq_n_u8(pg, chunk, '"');
        svbool_t bslash_match = svcmpeq_n_u8(pg, chunk, '\\');
        svbool_t ctrl_match = svcmplt_n_u8(pg, chunk, 0x20);

        svbool_t any_match = svorr_b_z(pg, quote_match, bslash_match);
        any_match = svorr_b_z(pg, any_match, ctrl_match);

        if (svptest_any(pg, any_match)) {
            /* Find first match */
            svbool_t first = svbrkb_b_z(pg, any_match);
            uint64_t count = svcntp_b8(pg, first);
            return pos + count;
        }

        pos += svcntb();
    }

    return len;
}

size_t find_structural_sve(const char *str, size_t len, uint64_t *mask) {
    if (len == 0) {
        *mask = 0;
        return 0;
    }

    /* For simplicity, process up to 64 bytes */
    size_t count = len < 64 ? len : 64;

    uint64_t m = 0;
    size_t pos = 0;

    while (pos < count) {
        svbool_t pg = svwhilelt_b8(pos, count);

        svuint8_t chunk = svld1_u8(pg, (const uint8_t *)(str + pos));

        svbool_t match = svcmpeq_n_u8(pg, chunk, '{');
        match = svorr_b_z(pg, match, svcmpeq_n_u8(pg, chunk, '}'));
        match = svorr_b_z(pg, match, svcmpeq_n_u8(pg, chunk, '['));
        match = svorr_b_z(pg, match, svcmpeq_n_u8(pg, chunk, ']'));
        match = svorr_b_z(pg, match, svcmpeq_n_u8(pg, chunk, ':'));
        match = svorr_b_z(pg, match, svcmpeq_n_u8(pg, chunk, ','));
        match = svorr_b_z(pg, match, svcmpeq_n_u8(pg, chunk, '"'));

        /* Convert predicate to bitmask */
        /* SVE doesn't have direct predicate-to-mask, iterate */
        svuint8_t ones = svdup_n_u8(1);
        svuint8_t zeros = svdup_n_u8(0);
        svuint8_t bits = svsel_u8(match, ones, zeros);

        uint8_t temp[64];
        svst1_u8(pg, temp, bits);

        size_t vl = svcntb();
        for (size_t i = 0; i < vl && pos + i < count; i++) {
            if (temp[i]) {
                m |= (1ULL << (pos + i));
            }
        }

        pos += vl;
    }

    *mask = m;
    return count;
}

int64_t parse_int_sve(const char *str, size_t len, size_t *consumed) {
    /* Use NEON implementation for now */
    return parse_int_neon(str, len, consumed);
}

#else
/* SVE not available at compile time */
size_t scan_string_sve(const char *str, size_t len) {
    return scan_string_neon(str, len);
}

size_t find_structural_sve(const char *str, size_t len, uint64_t *mask) {
    return find_structural_neon(str, len, mask);
}

int64_t parse_int_sve(const char *str, size_t len, size_t *consumed) {
    return parse_int_neon(str, len, consumed);
}
#endif /* __ARM_FEATURE_SVE */
#endif /* NO_SVE */

/* ============================================================================
 * SVE2 Implementation
 * ============================================================================ */

#ifndef NO_SVE
#if defined(__ARM_FEATURE_SVE2)

size_t scan_string_sve2(const char *str, size_t len) {
    /* SVE2 implementation - same as SVE for string scanning */
    return scan_string_sve(str, len);
}

size_t find_structural_sve2(const char *str, size_t len, uint64_t *mask) {
    /* SVE2 implementation - same as SVE */
    return find_structural_sve(str, len, mask);
}

int64_t parse_int_sve2(const char *str, size_t len, size_t *consumed) {
    return parse_int_sve(str, len, consumed);
}

#else
/* SVE2 not available */
size_t scan_string_sve2(const char *str, size_t len) {
    return scan_string_neon(str, len);
}

size_t find_structural_sve2(const char *str, size_t len, uint64_t *mask) {
    return find_structural_neon(str, len, mask);
}

int64_t parse_int_sve2(const char *str, size_t len, size_t *consumed) {
    return parse_int_neon(str, len, consumed);
}
#endif /* __ARM_FEATURE_SVE2 */
#endif /* NO_SVE */

#endif /* JSON_ARCH_ARM64 */
