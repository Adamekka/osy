#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <unistd.h>
#include <vector>

// Send every byte in data. The descriptor is borrowed and stays open.
inline auto write_all(const int32_t descriptor, const std::string& data)
    -> void {
    auto sent{size_t{0}};
    // A write may send only part of the data, so resume at the next byte.
    while (sent < data.size()) {
        const auto written{
            write(descriptor, data.data() + sent, data.size() - sent)
        };
        assert(written > 0);
        sent += static_cast<size_t>(written);
    }
}

[[nodiscard]] inline auto split_words(const std::string& text)
    -> std::vector<std::string> {
    auto words{std::vector<std::string>{}};
    const auto whitespace{std::string{" \t\r\n\f\v"}};
    auto begin{text.find_first_not_of(whitespace)};
    while (begin != std::string::npos) {
        auto end{text.find_first_of(whitespace, begin)};
        if (end == std::string::npos) {
            end = text.size();
        }
        words.emplace_back(text.substr(begin, end - begin));
        begin = text.find_first_not_of(whitespace, end);
    }
    return words;
}
