#pragma once
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>

namespace winindex {
namespace TokenMatcher {

inline bool IsTokenSep(wchar_t c) {
    return c == L' ' || c == L'_' || c == L'-' || c == L'.';
}

// Returns true if query contains at least one separator character.
// Used to gate token-set matching — single-word queries skip this path.
bool QueryHasSeparators(const std::wstring& query);

// Split a (pre-lowercased) wstring on separator chars into wstring_view slices.
// Consecutive separators produce no empty tokens.
// The returned views reference `s`, which must outlive them.
std::vector<std::wstring_view> TokenizeView(const std::wstring& s);

// Returns true if every token in sortedQueryTokens is present in
// sortedFilenameTokens (exact equality). Both must be pre-sorted.
bool AllQueryTokensPresent(const std::vector<std::wstring_view>& sortedQueryTokens,
                           const std::vector<std::wstring_view>& sortedFilenameTokens);

}  // namespace TokenMatcher
}  // namespace winindex
