#pragma once
#include <string>
#include <cstdint>

namespace winindex {

// Runtime SIMD capability detection
struct SimdCaps {
    bool avx2;
    bool sse42;
};
SimdCaps DetectSimdCaps();

// Returns index of first occurrence of needle in haystack, or npos.
// Uses AVX2 or SSE4.2 depending on caps detected at runtime.
size_t SimdFindSubstring(const wchar_t* haystack, size_t haystackLen,
                          const wchar_t* needle,   size_t needleLen);

size_t SimdFindSubstringInsensitive(const wchar_t* haystack, size_t haystackLen,
                                     const wchar_t* needle,   size_t needleLen);

} // namespace winindex
