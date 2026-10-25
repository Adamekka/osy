#include "io.hpp"
#include "zoo_map.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// Forward terminal input and server output as bytes. The server parses
// commands; the user still starts the session by typing HELLO followed by a
// name.
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

    // events asks for readiness; poll() writes the result flags into revents.
    pollfd events[]{
        // events[0] watches keyboard input; events[1] watches server replies.
        {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0},
        {.fd = server_socket, .events = POLLIN, .revents = 0}
    };
    // One scratch buffer is enough because the two reads are handled in turn.
    char buffer[1024];

    constexpr const char* moves[]{
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
            const auto move{moves[std::rand() % 4]};
            write_all(server_socket, move, std::strlen(move));
            continue;
        }

        // revents is a bit mask, so several flags can be present at once.
        // Hangups can still have buffered data; read it before checking EOF.
        if (events[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            const auto received{read(server_socket, buffer, sizeof(buffer))};
            assert(received >= 0);
            if (received == 0) {
                break;
            }
            // read() adds no '\0'; display exactly the number of bytes
            // received.
            // write_all(STDOUT_FILENO, buffer, static_cast<size_t>(received));
            std::puts(buffer);
            // update_map(0, 0, buffer);
            // draw_map();
        }

        if (events[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            const auto typed{read(STDIN_FILENO, buffer, sizeof(buffer))};
            assert(typed >= 0);
            if (typed == 0) {
                // End outgoing input, but keep receiving the server's replies.
                const auto shutdown_result{shutdown(server_socket, SHUT_WR)};
                assert(shutdown_result == 0);
                // poll ignores negative descriptors, preventing repeated EOF
                // reads.
                events[0].fd = -1;
            } else {
                write_all(server_socket, buffer, static_cast<size_t>(typed));
            }
        }
    }

    close(server_socket);
    return 0;
}
