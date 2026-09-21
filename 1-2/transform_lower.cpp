#include "transform.hpp"
#include <cctype>
#include <cstdint>

auto transform_char(char c) -> char {
    return static_cast<char>(std::tolower(static_cast<int32_t>(c)));
}
