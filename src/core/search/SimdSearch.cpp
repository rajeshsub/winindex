#include "SimdSearch.h"

#include <emmintrin.h>  // SSE2 — baseline on x64 Windows, no /arch flag required
#include <intrin.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace winindex {

// Defined in SimdSearchAvx2.cpp, compiled with /arch:AVX2.
size_t Avx2Find(const wchar_t* hay, size_t hayLen, const wchar_t* needle, size_t needleLen);

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
// SSE2 — 8 wchar_t at a time; always compiled on x64, no /arch flag needed.
// ---------------------------------------------------------------------------
static size_t Sse2Find(const wchar_t* hay, size_t hayLen, const wchar_t* needle, size_t needleLen) {
    if (needleLen == 0)
        return 0;
    if (needleLen > hayLen)
        return std::wstring::npos;

    const __m128i vFirst = _mm_set1_epi16(static_cast<short>(needle[0]));
    const wchar_t* p = hay;
    const wchar_t* end = hay + hayLen - needleLen + 1;

    while (p + 8 <= end) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
        __m128i cmp = _mm_cmpeq_epi16(chunk, vFirst);
        unsigned int mask = static_cast<unsigned int>(_mm_movemask_epi8(cmp));

        while (mask) {
            unsigned long bit;
            _BitScanForward(&bit, mask);
            size_t off = bit / 2;  // byte index → wchar_t index
            if (wmemcmp(p + off, needle, needleLen) == 0)
                return static_cast<size_t>(p + off - hay);
            // Clear both bytes of this wchar_t's match bits
            mask &= mask - 1;
            mask &= mask - 1;
        }
        p += 8;
    }

    while (p < end) {
        if (*p == needle[0] && wmemcmp(p, needle, needleLen) == 0)
            return static_cast<size_t>(p - hay);
        ++p;
    }
    return std::wstring::npos;
}

// ---------------------------------------------------------------------------
// Public API — runtime dispatch
// ---------------------------------------------------------------------------
size_t SimdFindSubstring(const wchar_t* haystack, size_t haystackLen, const wchar_t* needle,
                         size_t needleLen) {
    if (g_simdCaps.avx2)
        return Avx2Find(haystack, haystackLen, needle, needleLen);
    return Sse2Find(haystack, haystackLen, needle, needleLen);
}

size_t SimdFindSubstringInsensitive(const wchar_t* haystack, size_t haystackLen,
                                    const wchar_t* needle, size_t needleLen) {
    return ScalarFindInsensitive(haystack, haystackLen, needle, needleLen);
}

}  // namespace winindex
