#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <sys/socket.h>

constexpr auto pizza_capacity{size_t{1024}};

[[nodiscard]] inline auto
read_line(const int32_t descriptor, char (&line)[pizza_capacity])
    -> bool {
    auto used{size_t{0}};
    while (true) {
        char byte;
        const auto received{recv(descriptor, &byte, sizeof(byte), 0)};
        if (received == -1 && errno == EINTR) {
            continue;
        }
        if (received <= 0) {
            return false;
        }
        if (byte == '\n') {
            line[used] = '\0';
            return true;
        }
        if (byte == '\0' || used == pizza_capacity - 1) {
            return false;
        }
        line[used++] = byte;
    }
}

[[nodiscard]] inline auto
send_line(const int32_t descriptor, const std::string_view text) -> bool {
    const auto message{std::string{text} + '\n'};
    auto sent{size_t{0}};
    while (sent < message.size()) {
        // A disconnected client must not terminate the server through SIGPIPE.
        const auto written{send(
            descriptor,
            message.data() + sent,
            message.size() - sent,
            MSG_NOSIGNAL
        )};
        if (written == -1 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return false;
        }
        sent += static_cast<size_t>(written);
    }
    return true;
}

[[nodiscard]] inline auto receive_ok(const int32_t descriptor) -> bool {
    char reply[pizza_capacity];
    return read_line(descriptor, reply) && std::strcmp(reply, "OK") == 0;
}
