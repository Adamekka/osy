#include "log.hpp"
#include <cstdio>
#include <cstring>

auto main(int argc, char* argv[]) -> int {
    if (argc > 3) {
        std::printf("Arguments expected.");
    }

    const auto* const flag{argv[1]};

    const auto* const path{argv[2]};

    if (strcmp(flag, "-a") == 0) {
        log_access(path);
    } else if (strcmp(flag, "-s") == 0) {
        log_stat(path);
    } else if (strcmp(flag, "-l") == 0) {
        log_lstat(path);
    } else if (strcmp(flag, "-f") == 0) {
        log_fstat(path);
    }
}
