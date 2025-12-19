/*
 * json-asm: CPU feature detection
 */

#include "internal.h"

#if defined(JSON_ARCH_X86_64)
#include <cpuid.h>

uint32_t cpu_detect_features(void) {
    uint32_t features = 0;
    unsigned int eax, ebx, ecx, edx;

    /* Check for basic CPUID support */
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        return features;
    }

    /* SSE4.2 */
    if (ecx & (1 << 20)) {
        features |= JSON_CPU_SSE42;
    }

    /* POPCNT */
    if (ecx & (1 << 23)) {
        features |= JSON_CPU_POPCNT;
    }

    /* Check extended features (leaf 7) */
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        /* AVX2 */
        if (ebx & (1 << 5)) {
            features |= JSON_CPU_AVX2;
        }

        /* BMI1 */
        if (ebx & (1 << 3)) {
            features |= JSON_CPU_BMI1;
        }

        /* BMI2 */
        if (ebx & (1 << 8)) {
            features |= JSON_CPU_BMI2;
        }

        /* AVX-512F */
        if (ebx & (1 << 16)) {
            features |= JSON_CPU_AVX512F;
        }

        /* AVX-512BW */
        if (ebx & (1 << 30)) {
            features |= JSON_CPU_AVX512BW;
        }

        /* AVX-512VL */
        if (ebx & (1 << 31)) {
            features |= JSON_CPU_AVX512VL;
        }
    }

    /* LZCNT - check extended CPUID */
    if (__get_cpuid(0x80000001, &eax, &ebx, &ecx, &edx)) {
        if (ecx & (1 << 5)) {
            features |= JSON_CPU_LZCNT;
        }
    }

    return features;
}

#elif defined(JSON_ARCH_ARM64)

#if defined(__linux__)
#include <sys/auxv.h>
#include <asm/hwcap.h>

uint32_t cpu_detect_features(void) {
    uint32_t features = JSON_CPU_NEON; /* Always present on AArch64 */

    unsigned long hwcap = getauxval(AT_HWCAP);
    unsigned long hwcap2 = getauxval(AT_HWCAP2);

    /* SVE */
    #ifdef HWCAP_SVE
    if (hwcap & HWCAP_SVE) {
        features |= JSON_CPU_SVE;
    }
    #endif

    /* SVE2 */
    #ifdef HWCAP2_SVE2
    if (hwcap2 & HWCAP2_SVE2) {
        features |= JSON_CPU_SVE2;
    }
    #endif

    /* Dot Product */
    #ifdef HWCAP_ASIMDDP
    if (hwcap & HWCAP_ASIMDDP) {
        features |= JSON_CPU_DOTPROD;
    }
    #endif

    /* SHA3 */
    #ifdef HWCAP_SHA3
    if (hwcap & HWCAP_SHA3) {
        features |= JSON_CPU_SHA3;
    }
    #endif

    return features;
}

#elif defined(__APPLE__)
#include <sys/sysctl.h>

uint32_t cpu_detect_features(void) {
    uint32_t features = JSON_CPU_NEON; /* Always present on Apple Silicon */

    /* Apple Silicon currently doesn't have SVE */
    /* But we check anyway for future compatibility */
    int64_t val = 0;
    size_t size = sizeof(val);

    if (sysctlbyname("hw.optional.arm.FEAT_DotProd", &val, &size, NULL, 0) == 0) {
        if (val) features |= JSON_CPU_DOTPROD;
    }

    /* Reset size for next call */
    size = sizeof(val);
    if (sysctlbyname("hw.optional.arm.FEAT_SHA3", &val, &size, NULL, 0) == 0) {
        if (val) features |= JSON_CPU_SHA3;
    }

    return features;
}

#else
/* Generic ARM64 - assume only NEON */
uint32_t cpu_detect_features(void) {
    return JSON_CPU_NEON;
}
#endif

#else
/* Scalar fallback */
uint32_t cpu_detect_features(void) {
    return 0;
}
#endif

/* ============================================================================
 * Standalone detection tool
 * ============================================================================ */

#ifdef DETECT_MAIN
int main(void) {
    uint32_t features = cpu_detect_features();

    printf("Detected CPU features:\n");

#if defined(JSON_ARCH_X86_64)
    printf("  Architecture: x86-64\n");
    if (features & JSON_CPU_SSE42)    printf("  SSE4.2\n");
    if (features & JSON_CPU_AVX2)     printf("  AVX2\n");
    if (features & JSON_CPU_AVX512F)  printf("  AVX-512F\n");
    if (features & JSON_CPU_AVX512BW) printf("  AVX-512BW\n");
    if (features & JSON_CPU_AVX512VL) printf("  AVX-512VL\n");
    if (features & JSON_CPU_BMI1)     printf("  BMI1\n");
    if (features & JSON_CPU_BMI2)     printf("  BMI2\n");
    if (features & JSON_CPU_POPCNT)   printf("  POPCNT\n");
    if (features & JSON_CPU_LZCNT)    printf("  LZCNT\n");
#elif defined(JSON_ARCH_ARM64)
    printf("  Architecture: ARM64\n");
    if (features & JSON_CPU_NEON)     printf("  NEON\n");
    if (features & JSON_CPU_SVE)      printf("  SVE\n");
    if (features & JSON_CPU_SVE2)     printf("  SVE2\n");
    if (features & JSON_CPU_DOTPROD)  printf("  DOTPROD\n");
    if (features & JSON_CPU_SHA3)     printf("  SHA3\n");
#else
    printf("  Architecture: Scalar\n");
#endif

    printf("\nSelected implementation: ");
#if defined(JSON_ARCH_X86_64)
    #ifndef NO_AVX512
    if ((features & (JSON_CPU_AVX512F | JSON_CPU_AVX512BW)) ==
        (JSON_CPU_AVX512F | JSON_CPU_AVX512BW)) {
        printf("AVX-512\n");
    } else
    #endif
    if (features & JSON_CPU_AVX2) {
        printf("AVX2\n");
    } else {
        printf("SSE4.2\n");
    }
#elif defined(JSON_ARCH_ARM64)
    #ifndef NO_SVE
    if (features & JSON_CPU_SVE2) {
        printf("SVE2\n");
    } else if (features & JSON_CPU_SVE) {
        printf("SVE\n");
    } else
    #endif
    {
        printf("NEON\n");
    }
#else
    printf("Scalar\n");
#endif

    return 0;
}
#endif
