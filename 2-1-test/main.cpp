#include "io.hpp"
#include "zoo_map.hpp"
#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

// The final byte is reserved for '\0' in a parsed command.
constexpr auto record_capacity{size_t{1024}};
uint8_t map[map_size * map_size];
bool debug = false;

// Keep one buffer per connection. Unread bytes are in [position, length).
struct InputBuffer final {
    char bytes[record_capacity]; // read() fills raw bytes without a terminator.
    size_t position{0};
    size_t length{0};
};

// Return a listening socket for 127.0.0.1:12345. The caller owns the
// descriptor; accept() will create separate descriptors for individual
// visitors.
[[nodiscard]] auto create_server_socket() -> int32_t {
    const auto server_socket{socket(AF_INET, SOCK_STREAM, 0)};
    assert(server_socket != -1);

    auto address{sockaddr_in{}};
    address.sin_family = AF_INET;
    // Socket addresses store the port and IPv4 address in network byte order.
    address.sin_port = htons(12345);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // bind accepts a generic address pointer; address contains IPv4 fields.
    const auto bind_result{bind(
        server_socket,
        reinterpret_cast<const sockaddr*>(&address),
        sizeof(address)
    )};
    assert(bind_result == 0);

    const auto listen_result{listen(server_socket, SOMAXCONN)};
    assert(listen_result == 0);
    return server_socket;
}

// Return one command without '\n', terminated with '\0', or false on EOF.
// Keep input between calls. The array reference preserves command's capacity.
[[nodiscard]] auto read_command(
    const int32_t client_socket,
    InputBuffer& input,
    char (&command)[record_capacity]
) -> bool {
    auto client_event{
        pollfd{.fd = client_socket, .events = POLLIN, .revents = 0}
    };
    auto used{size_t{0}};
    auto discard_command{false};

    while (true) {
        // Reuse unread bytes before polling again. One read may contain several
        // commands, and returning one command must preserve the remaining
        // bytes.
        if (input.position == input.length) {
            // -1 waits indefinitely; read() then distinguishes data from EOF.
            const auto poll_result{poll(&client_event, 1, -1)};
            assert(poll_result > 0);

            const auto received{
                read(client_socket, input.bytes, sizeof(input.bytes))
            };
            assert(received >= 0);
            if (received == 0) {
                // EOF ends the session; an unfinished final command is ignored.
                return false;
            }

            input.position = 0;
            input.length = static_cast<size_t>(received);
        }

        const auto byte{input.bytes[input.position++]};
        if (discard_command) {
            // Discard through the newline so a bad line's suffix is not parsed
            // as a new command, even when the line spans multiple reads.
            if (byte == '\n') {
                discard_command = false;
            }
            continue;
        }

        if (byte == '\n') {
            command[used] = '\0';
            return true;
        }

        // Reserve space for '\0'. Oversized lines and embedded terminators are
        // invalid input, so discard the line and let the visitor try again.
        if (used == sizeof(command) - 1 || byte == '\0') {
            used = 0;
            discard_command = true;
            continue;
        }

        command[used++] = byte;
    }
}

// Run one external query and wait for its output. Keep the visitor's socket
// open so serve_player can handle the next command after this function returns.
// Requires SIGCHLD's default behavior, restored in the connection child in
// main.
auto send_animal_info(const int32_t client_socket, const char* const animal)
    -> void {
    const auto info_child{fork()};
    assert(info_child != -1);
    if (info_child == 0) {
        // Change stdout only in the INFO child. The visitor keeps its own
        // stdout.
        const auto redirect_result{dup2(client_socket, STDOUT_FILENO)};
        assert(redirect_result != -1);
        // stdout now holds another reference to this socket.
        close(client_socket);

        // argv[0] is the program name, argv[1] is animal; nullptr ends the
        // list. The relative executable path is resolved from the server's
        // working dir.
        execl("./zoo_info", "zoo_info", animal, static_cast<char*>(nullptr));
        // Success replaces this process. On failure, stop even if asserts are
        // off.
        assert(false && "execl zoo_info failed");
        _exit(EXIT_FAILURE);
    }

    // Only this visitor waits. Other visitors and the listener keep running.
    // Waiting also prevents INFO output from mixing with the next reply.
    const auto waited{waitpid(info_child, nullptr, 0)};
    assert(waited == info_child);
}

auto send_area(int x, int y, char area[]) -> void {
    int index = 0;

    for (auto dy{int32_t{-1}}; dy <= 1; dy++) {
        for (auto dx{int32_t{-1}}; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;

            int value = 0;
            if (nx >= 0 && nx < map_size && ny >= 0 && ny < map_size) {
                value = map[ny * map_size + nx];
            }

            if (index > 0) {
                area[index++] = ',';
            }

            area[index++] = '0' + value;
        }
    }
    area[index] = '\0';
}

auto get_map_str(char map_str[]) -> void {
    int index = 0;

    for (auto dy{int32_t{0}}; dy < map_size; dy++) {
        for (auto dx{int32_t{0}}; dx < map_size; dx++) {
            int nx = dx;
            int ny = dy;

            int value = map[ny * map_size + nx];

            if (index > 0) {
                map_str[index++] = ',';
            }

            map_str[index++] = '0' + value;
        }
    }

    map_str[index] = '\0';
}

// Run in a connection child. State belongs to one visitor, and HELLO must fill
// it before other commands use it. The caller closes the socket on return.
auto serve_player(const int32_t client_socket) -> void {
    std::puts("Client connected");

    // Keep unread bytes between read_command calls, including after a newline.
    auto input{InputBuffer{}};
    char command[record_capacity];

    // HELLO fills these fields once; they persist for later commands.
    char name[record_capacity];
    int32_t x;
    int32_t y;
    auto registered{false};

    write_all(
        client_socket, "Zadej jmeno", static_cast<size_t>(sizeof("Zadej jmeno"))
    );

    while (read_command(client_socket, input, command)) {
        std::printf("Command: %s\n", command);

        // Leave room for the longer WELCOME reply as well as the shorter POS.
        char response[sizeof(name) + 256];
        int32_t formatted;
        if (registered) {
            if (std::strcmp(command, "QUIT") == 0) {
                formatted = std::snprintf(
                    response, sizeof(response), "BYE %s\n", name
                );
                assert(formatted > 0);
                write_all(
                    client_socket, response, static_cast<size_t>(formatted)
                );
                // main closes this visitor's socket and exits the child.
                return;
            }

            if (std::strncmp(command, "INFO ", 5) == 0) {
                char animal[record_capacity];
                char extra;
                // A second conversion detects extra arguments, as with HELLO.
                const auto matched{
                    std::sscanf(command, "INFO %1023s %c", animal, &extra)
                };
                if (matched != 1) {
                    continue;
                }

                send_animal_info(client_socket, animal);
                // The helper sent the reply; skip the common POS reply below.
                continue;
            }

            // NORTH increases y; EAST increases x. WHERE uses the same POS
            // reply. At an edge, keep the coordinate unchanged and still reply
            // POS.
            if (std::strcmp(command, "MOVE NORTH") == 0) {
                if (y < map_size - 1) {
                    ++y;
                }

                formatted = std::snprintf(
                    response, sizeof(response), "POS %d %d\n", x, y
                );
            } else if (std::strcmp(command, "MOVE SOUTH") == 0) {
                if (y > 0) {
                    --y;
                }

                formatted = std::snprintf(
                    response, sizeof(response), "POS %d %d\n", x, y
                );
            } else if (std::strcmp(command, "MOVE EAST") == 0) {
                if (x < map_size - 1) {
                    ++x;
                }

                formatted = std::snprintf(
                    response, sizeof(response), "POS %d %d\n", x, y
                );
            } else if (std::strcmp(command, "MOVE WEST") == 0) {
                if (x > 0) {
                    --x;
                }

                formatted = std::snprintf(
                    response, sizeof(response), "POS %d %d\n", x, y
                );
            } else if (std::strcmp(command, "AREA") == 0) {
                char area[18];
                send_area(x, y, area);

                formatted = std::snprintf(
                    response,
                    sizeof(response),
                    "POS %d %d AREA %s\n",
                    x,
                    y,
                    area
                );

            } else if (std::strcmp(command, "WHERE") == 0) {
                formatted = std::snprintf(
                    response, sizeof(response), "POS %d %d\n", x, y
                );
            } else {
                continue;
            }
        } else {
            // Require HELLO followed by a space and one name. A second
            // conversion detects extra arguments; the width leaves room for
            // the terminator.
            char extra;
            const auto matched{
                std::sscanf(command, "HELLO %1023s %c", name, &extra)
            };
            if (std::strncmp(command, "HELLO ", 6) != 0 || matched != 1) {
                // Stay unregistered so a bad command permits a valid retry.
                continue;
            }

            // fork copies PRNG state, so seed each visitor with its own PID.
            std::srand(static_cast<uint32_t>(getpid()));
            x = std::rand() % map_size;
            y = std::rand() % map_size;
            registered = true;

            formatted = std::snprintf(
                response, sizeof(response), "WELCOME %s (%d,%d)\n", name, x, y
            );
        }

        assert(formatted > 0);
        // snprintf counts the text bytes and excludes the terminating '\0'.
        // write_all(client_socket, response, static_cast<size_t>(formatted));
        char map_str[200];
        get_map_str(map_str);
        // std::puts(map_str);
        update_map(x, y, map_str);
        draw_map();
        write_all(client_socket, map_str, static_cast<size_t>(formatted));
    }
}

// The parent only accepts and forks. Each child runs one visitor session.
auto main(const int32_t argc, char** const argv) -> int {
    std::srand(static_cast<uint32_t>(getpid()));

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        debug = true;
    }

    for (int i = 0; i < map_size; i++) {
        for (int j = 0; j < map_size; j++) {
            map[i * map_size + j] = rand() % 8 + 1;
        }
    }

    map_init(map_size, map_size);

    const auto server_socket{create_server_socket()};

    // Explicitly ignoring SIGCHLD lets the OS collect exited children instead
    // of leaving zombies while this parent waits for new connections.
    const auto child_signal_result{std::signal(SIGCHLD, SIG_IGN)};
    assert(child_signal_result != SIG_ERR);

    while (true) {
        // accept returns a new socket for one visitor and keeps the listener.
        // nullptr skips the peer's address output, which we do not need.
        const auto client_socket{accept(server_socket, nullptr, nullptr)};
        assert(client_socket != -1);

        // Both processes continue here: the child gets 0, the parent gets its
        // PID. Their separate descriptor tables refer to the same underlying
        // sockets.
        const auto child{fork()};
        assert(child != -1);

        if (child == 0) {
            // Only the parent accepts connections; closing this copy leaves
            // the parent's listening socket open.
            close(server_socket);

            // The parent auto-reaps visitors, but this child must wait for
            // zoo_info.
            const auto info_signal_result{std::signal(SIGCHLD, SIG_DFL)};
            assert(info_signal_result != SIG_ERR);

            serve_player(client_socket);
            close(client_socket);

            // End this child instead of returning to the parent's accept loop.
            return 0;
        }

        // Keeping this copy open would prevent the client from seeing EOF when
        // the child closes its copy. Release it and accept the next visitor.
        close(client_socket);
    }
}
