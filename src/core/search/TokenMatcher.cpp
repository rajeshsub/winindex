#include "TokenMatcher.h"

#include <algorithm>

namespace winindex {
namespace TokenMatcher {

bool QueryHasSeparators(const std::wstring& query) {
    return std::any_of(query.begin(), query.end(), IsTokenSep);
}

std::vector<std::wstring_view> TokenizeView(const std::wstring& s) {
    std::vector<std::wstring_view> tokens;
    const std::wstring_view sv(s);
    size_t start = 0;
    for (size_t i = 0; i <= sv.size(); ++i) {
        if (i == sv.size() || IsTokenSep(sv[i])) {
            if (i > start)
                tokens.emplace_back(sv.substr(start, i - start));
            start = i + 1;
        }
    }
    return tokens;
}

bool AllQueryTokensPresent(const std::vector<std::wstring_view>& sortedQueryTokens,
                           const std::vector<std::wstring_view>& sortedFilenameTokens) {
    if (sortedQueryTokens.empty())
        return false;
    return std::all_of(
        sortedQueryTokens.begin(), sortedQueryTokens.end(), [&](const std::wstring_view& qt) {
            return std::binary_search(sortedFilenameTokens.begin(), sortedFilenameTokens.end(), qt);
        });
}

}  // namespace TokenMatcher
}  // namespace winindex
