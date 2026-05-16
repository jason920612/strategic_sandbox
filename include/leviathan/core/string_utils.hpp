// Minimal string helpers used by core code.
//
// Intentionally tiny: M0.2 only needs trim() for GameDate::parse. New
// helpers should be added here only when a real caller materialises,
// per RFC-001 §4 ("no premature scaffolding").

#ifndef LEVIATHAN_CORE_STRING_UTILS_HPP
#define LEVIATHAN_CORE_STRING_UTILS_HPP

#include <string>
#include <string_view>

namespace leviathan::core::string_utils {

// Returns `text` with ASCII whitespace stripped from both ends.
// Whitespace = space, tab, CR, LF, VT, FF.
std::string trim(std::string_view text);

}  // namespace leviathan::core::string_utils

#endif  // LEVIATHAN_CORE_STRING_UTILS_HPP
