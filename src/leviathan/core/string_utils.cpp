#include "leviathan/core/string_utils.hpp"

#include <cctype>
#include <string>
#include <string_view>

namespace leviathan::core::string_utils {

namespace {

bool is_ws(unsigned char c) noexcept {
    // Stick to the C locale's notion of whitespace so trim() is
    // deterministic and identical on every platform.
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f';
}

}  // namespace

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && is_ws(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && is_ws(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

}  // namespace leviathan::core::string_utils
