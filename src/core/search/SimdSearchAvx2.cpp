#include <immintrin.h>
#include <intrin.h>

#include "SimdSearch.h"
#include <string>

// This file is compiled with /arch:AVX2 (MSVC) or -mavx2 (GCC/Clang).
// All code here may emit AVX2 instructions; only call Avx2Find after
// confirming AVX2 support via DetectSimdCaps().

namespace winindex {

size_t Avx2Find(const wchar_t* hay, size_t hayLen, const wchar_t* needle, size_t needleLen) {
    if (needleLen == 0)
        return 0;
    if (needleLen > hayLen)
        return std::wstring::npos;

    const __m256i vFirst = _mm256_set1_epi16(static_cast<short>(needle[0]));
    const wchar_t* p = hay;
    const wchar_t* end = hay + hayLen - needleLen + 1;

    while (p + 16 <= end) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
        __m256i cmp = _mm256_cmpeq_epi16(chunk, vFirst);
        uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(cmp));

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
        p += 16;
    }

    while (p < end) {
        if (*p == needle[0] && wmemcmp(p, needle, needleLen) == 0)
            return static_cast<size_t>(p - hay);
        ++p;
    }
    return std::wstring::npos;
}

}  // namespace winindex
