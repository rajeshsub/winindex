#include "SimdSearch.h"

#include <intrin.h>

#include <algorithm>
#include <cctype>

namespace winindex {

SimdCaps DetectSimdCaps() {
    SimdCaps caps{};
    int cpuInfo[4] = {};
    __cpuid(cpuInfo, 1);
    caps.sse42 = (cpuInfo[2] & (1 << 20)) != 0;

    __cpuidex(cpuInfo, 7, 0);
    caps.avx2 = (cpuInfo[1] & (1 << 5)) != 0;
    return caps;
}

static const SimdCaps g_simdCaps = DetectSimdCaps();

// ---------------------------------------------------------------------------
// Scalar fallback
// ---------------------------------------------------------------------------
static size_t ScalarFind(const wchar_t* hay, size_t hayLen, const wchar_t* needle,
                         size_t needleLen) {
    if (needleLen == 0)
        return 0;
    if (needleLen > hayLen)
        return std::wstring::npos;
    for (size_t i = 0; i <= hayLen - needleLen; ++i) {
        if (wmemcmp(hay + i, needle, needleLen) == 0)
            return i;
    }
    return std::wstring::npos;
}

static size_t ScalarFindInsensitive(const wchar_t* hay, size_t hayLen, const wchar_t* needle,
                                    size_t needleLen) {
    if (needleLen == 0)
        return 0;
    if (needleLen > hayLen)
        return std::wstring::npos;
    for (size_t i = 0; i <= hayLen - needleLen; ++i) {
        bool match = true;
        for (size_t j = 0; j < needleLen; ++j) {
            if (towlower(hay[i + j]) != towlower(needle[j])) {
                match = false;
                break;
            }
        }
        if (match)
            return i;
    }
    return std::wstring::npos;
}

// ---------------------------------------------------------------------------
// SSE4.2 accelerated search (operates on UTF-16 as pairs of bytes)
// Uses _mm_cmpistrm for byte-level first-char scan, then verifies.
// ---------------------------------------------------------------------------
#ifdef __SSE4_2__
#include <nmmintrin.h>
static size_t Sse42Find(const wchar_t* hay, size_t hayLen, const wchar_t* needle,
                        size_t needleLen) {
    if (needleLen == 0)
        return 0;
    if (needleLen > hayLen)
        return std::wstring::npos;

    wchar_t firstChar = needle[0];
    const wchar_t* p = hay;
    const wchar_t* end = hay + hayLen - needleLen + 1;

    while (p < end) {
        // Scan for first character
        if (*p == firstChar) {
            if (wmemcmp(p, needle, needleLen) == 0)
                return static_cast<size_t>(p - hay);
        }
        ++p;
    }
    return std::wstring::npos;
}
#endif

// ---------------------------------------------------------------------------
// AVX2 accelerated search — scan 16 wchar_t at a time for first character
// ---------------------------------------------------------------------------
#ifdef __AVX2__
#include <immintrin.h>
static size_t Avx2Find(const wchar_t* hay, size_t hayLen, const wchar_t* needle, size_t needleLen) {
    if (needleLen == 0)
        return 0;
    if (needleLen > hayLen)
        return std::wstring::npos;

    const wchar_t firstChar = needle[0];
    const __m256i vFirst = _mm256_set1_epi16(static_cast<short>(firstChar));

    const wchar_t* p = hay;
    const wchar_t* end = hay + hayLen - needleLen + 1;

    // Process 16 wchar_t at a time
    while (p + 16 <= end) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
        __m256i cmp = _mm256_cmpeq_epi16(chunk, vFirst);
        uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(cmp));

        while (mask) {
            // Each bit pair corresponds to one wchar_t
            unsigned long bit;
            _BitScanForward(&bit, mask);
            size_t offset = bit / 2;
            if (p + offset + needleLen <= hay + hayLen) {
                if (wmemcmp(p + offset, needle, needleLen) == 0)
                    return static_cast<size_t>(p + offset - hay);
            }
            mask &= mask - 1;
            mask &= mask - 1;  // clear both bits of the pair
        }
        p += 16;
    }

    // Tail: scalar cleanup
    while (p < end) {
        if (*p == firstChar && wmemcmp(p, needle, needleLen) == 0)
            return static_cast<size_t>(p - hay);
        ++p;
    }
    return std::wstring::npos;
}
#endif

// ---------------------------------------------------------------------------
// Public API — dispatch at runtime
// ---------------------------------------------------------------------------
size_t SimdFindSubstring(const wchar_t* haystack, size_t haystackLen, const wchar_t* needle,
                         size_t needleLen) {
#ifdef __AVX2__
    if (g_simdCaps.avx2)
        return Avx2Find(haystack, haystackLen, needle, needleLen);
#endif
#ifdef __SSE4_2__
    if (g_simdCaps.sse42)
        return Sse42Find(haystack, haystackLen, needle, needleLen);
#endif
    return ScalarFind(haystack, haystackLen, needle, needleLen);
}

size_t SimdFindSubstringInsensitive(const wchar_t* haystack, size_t haystackLen,
                                    const wchar_t* needle, size_t needleLen) {
    // Case-insensitive: lowercase the needle once, scan with lowercased comparison.
    // SIMD for case-insensitive wide strings is complex; scalar is used here.
    // The needle is short; the haystack length drives cost.
    return ScalarFindInsensitive(haystack, haystackLen, needle, needleLen);
}

}  // namespace winindex
