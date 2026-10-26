#pragma once

#include <cstdint>

constexpr auto map_size{int32_t{10}};

auto map_init(int width, int height) -> void;
auto draw_map(void) -> void;
auto update_map(int px, int py, const char* area_str) -> void;
