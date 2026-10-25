#pragma once

#include <cassert>
#include <cstdint>
#include <unistd.h>

// Send exactly size bytes. The descriptor is borrowed and stays open.
// data may contain raw bytes; this function does not look for a '\0'.
inline auto write_all(
    const int32_t descriptor, const char* const data, const size_t size
) -> void {
    auto sent{size_t{0}};
    // A write may send only part of the data, so resume at the next byte.
    while (sent < size) {
        const auto written{write(descriptor, data + sent, size - sent)};
        assert(written > 0);
        sent += static_cast<size_t>(written);
    }
}
