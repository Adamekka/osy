#include "io.hpp"
#include "zoo_map.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// Prompt for the name, then forward commands and draw received map areas.
auto main(const int32_t argc, char** const argv) -> int {
    assert(argc == 3 && "Usage: zoo_client <IPv4_address> <port>");

    map_init(map_size, map_size);

    char* port_end;
    const auto port{std::strtol(argv[2], &port_end, 10)};
    // Reject trailing text as well as ports outside the TCP port range.
    assert(*port_end == '\0' && port > 0 && port <= 65535);

    const auto server_socket{socket(AF_INET, SOCK_STREAM, 0)};
    assert(server_socket != -1);

    auto address{sockaddr_in{}};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));
    // Convert dotted IPv4 text to the binary address required by connect().
    const auto address_result{inet_pton(AF_INET, argv[1], &address.sin_addr)};
    assert(address_result == 1);

    // connect takes a generic address pointer; address contains IPv4 fields.
    const auto connect_result{connect(
        server_socket,
        reinterpret_cast<const sockaddr*>(&address),
        sizeof(address)
    )};
    assert(connect_result == 0);

    std::fputs("Jméno: ", stdout);
    std::fflush(stdout);
    // Keep synchronized std::cin unbuffered so poll can read later piped
    // commands.
    std::setvbuf(stdin, nullptr, _IONBF, 0);
    auto name{std::string{}};
    const auto name_read{static_cast<bool>(std::getline(std::cin, name))};
    assert(name_read);
    if (!name.empty() && name.back() == '\r') {
        name.pop_back();
    }
    write_all(server_socket, "HELLO " + name + "\n");

    // events asks for readiness; poll() writes the result flags into revents.
    pollfd events[]{
        // events[0] watches keyboard input; events[1] watches server replies.
        {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0},
        {.fd = server_socket, .events = POLLIN, .revents = 0}
    };
    // One scratch buffer is enough because the two reads are handled in turn.
    auto buffer{std::string(1024, '\0')};
    auto pending{std::string{}};

    const std::string moves[]{
        "MOVE NORTH\n", "MOVE SOUTH\n", "MOVE WEST\n", "MOVE EAST\n"
    };
    // Seed once so each client process has its own random movement sequence.
    std::srand(static_cast<uint32_t>(getpid()));

    while (true) {
        // Each event starts a new five-second wait, including a server reply.
        // After stdin EOF, wait only for replies because sending is shut down.
        const auto poll_result{poll(events, 2, events[0].fd < 0 ? -1 : 5000)};
        assert(poll_result >= 0);

        // Zero means neither descriptor became ready within five seconds.
        if (poll_result == 0) {
            write_all(server_socket, moves[std::rand() % 4]);
            continue;
        }

        // revents is a bit mask, so several flags can be present at once.
        // Hangups can still have buffered data; read it before checking EOF.
        if (events[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            const auto received{
                read(server_socket, buffer.data(), buffer.size())
            };
            assert(received >= 0);
            if (received == 0) {
                break;
            }
            // TCP can split a reply or combine several replies in one read.
            pending.append(buffer, 0, static_cast<size_t>(received));
            while (true) {
                const auto newline{pending.find('\n')};
                if (newline == std::string::npos) {
                    break;
                }
                const auto line{pending.substr(0, newline)};
                pending.erase(0, newline + 1);
                std::puts(line.c_str());

                const auto fields{split_words(line)};
                if (fields.size() < 5 || fields[0] != "POS" || fields[3] != "AREA") {
                    continue;
                }

                // Reject trailing text because from_chars also accepts numeric prefixes.
                int32_t x;
                const auto& x_text{fields[1]};
                const auto x_end{x_text.data() + x_text.size()};
                const auto x_result{std::from_chars(x_text.data(), x_end, x)};
                if (x_result.ec != std::errc{} || x_result.ptr != x_end) {
                    continue;
                }

                int32_t y;
                const auto& y_text{fields[2]};
                const auto y_end{y_text.data() + y_text.size()};
                const auto y_result{std::from_chars(y_text.data(), y_end, y)};
                if (y_result.ec != std::errc{} || y_result.ptr != y_end) {
                    continue;
                }

                update_map(x, y, fields[4].c_str());
                draw_map();
            }
        }

        if (events[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            const auto typed{read(STDIN_FILENO, buffer.data(), buffer.size())};
            assert(typed >= 0);
            if (typed == 0) {
                // End outgoing input, but keep receiving the server's replies.
                const auto shutdown_result{shutdown(server_socket, SHUT_WR)};
                assert(shutdown_result == 0);
                // poll ignores negative descriptors, preventing repeated EOF
                // reads.
                events[0].fd = -1;
            } else {
                write_all(
                    server_socket, buffer.substr(0, static_cast<size_t>(typed))
                );
            }
        }
    }

    close(server_socket);
    return 0;
}
