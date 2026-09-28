#include "log.hpp"
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

auto log_access(const char* const path) -> void {
    if (access(path, R_OK) == 0) {
        std::printf("File %s is readable: YES\n", path);
    } else {
        std::printf("File %s is readable: NO\n", path);
    }
}

auto log_stat(const char* const path) -> void {
    struct stat info{};

    if (stat(path, &info) == 0) {
        std::printf("File:\t\t%s\n", path);
        std::printf("Size:\t\t%lld\n", info.st_size);
        std::printf("UID:\t\t%u\n", info.st_uid);
        std::printf("GID:\t\t%u\n", info.st_gid);
        std::printf("Hardlinks:\t%hu\n", info.st_nlink);
    }
}

auto log_lstat(const char* const path) -> void {
    struct stat info{};

    if (lstat(path, &info) == 0) {
        std::printf("File:\t\t%s\n", path);
        std::printf("Size:\t\t%lld\n", info.st_size);
        std::printf("File type:\t%hu\n", info.st_mode);
        std::printf("inode:\t\t%llu\n", info.st_ino);
    }
}

auto log_fstat(const char* const path) -> void {
    const auto file_descriptor{open(path, O_RDONLY)};

    struct stat info{};

    if (fstat(file_descriptor, &info) == 0) {
        std::printf("File:\t\t%s\n", path);
        std::printf("Size:\t\t%lld\n", info.st_size);
        std::printf("File type:\t%hu\n", info.st_mode);
        std::printf("inode:\t\t%llu\n", info.st_ino);
    }

    close(file_descriptor);
}
