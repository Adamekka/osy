#pragma once

auto log_access(const char* path) -> void;
auto log_stat(const char* path) -> void;
auto log_lstat(const char* path) -> void;
auto log_fstat(const char* path) -> void;
