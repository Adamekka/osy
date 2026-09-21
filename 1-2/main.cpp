#include "transform.hpp"
#include <cstdio>

auto main() -> int {
    for (auto c{getchar()}; c != EOF; c = getchar()) {
        const auto transformed{transform_char(static_cast<char>(c))};
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
        std::printf("%c", transformed);
    }
}
