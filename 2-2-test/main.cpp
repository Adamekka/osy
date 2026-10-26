#include "io.hpp"
#include "zoo_map.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

// Keep the original protocol limit of 1023 bytes per command.
constexpr auto record_capacity{size_t{1024}};
uint8_t zoo_map[map_size][map_size];
bool debug = false;

// Global storage is shared by all client threads. Synchronization comes in
// the next assignment, so concurrent changes are not safe yet.
auto clients{std::vector<int32_t>{}};

// Keep one buffer per connection. Unread bytes start at position.
struct InputBuffer final {
    std::string bytes;
    size_t position{0};
};

auto broadcast_message(
    const int32_t source_descriptor,
    const std::string& name,
    const std::string& text
) -> void {
    const auto message{name + ": " + text + "\n"};
    std::fputs(message.c_str(), stdout);
    for (const auto client_socket : clients) {
        // if dest != src
        if (client_socket != source_descriptor) {
            write_all(client_socket, message);
        }
    }
}

auto serve_console([[maybe_unused]] void* const argument) -> void* {
    auto text{std::string{}};
    while (std::getline(std::cin, text)) {
        // stdin has its own open descriptor, so every client receives this.
        broadcast_message(STDIN_FILENO, "Server", text);
    }
    assert(std::cin.eof());
    // Console EOF ends this thread while the server keeps accepting clients.
    return nullptr;
}

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

// Return one command without '\n', or false on EOF. Keep unread input.
[[nodiscard]] auto read_command(
    const int32_t client_socket, InputBuffer& input, std::string& command
) -> bool {
    auto client_event{
        pollfd{.fd = client_socket, .events = POLLIN, .revents = 0}
    };
    command.clear();
    auto discard_command{false};

    while (true) {
        // Reuse unread bytes before polling again. One read may contain several
        // commands, and returning one command must preserve the remaining
        // bytes.
        if (input.position == input.bytes.size()) {
            // -1 waits indefinitely; read() then distinguishes data from EOF.
            const auto poll_result{poll(&client_event, 1, -1)};
            assert(poll_result > 0);

            input.bytes.resize(record_capacity);
            const auto received{
                read(client_socket, input.bytes.data(), input.bytes.size())
            };
            assert(received >= 0);
            if (received == 0) {
                // EOF ends the session; an unfinished final command is ignored.
                return false;
            }

            input.position = 0;
            input.bytes.resize(static_cast<size_t>(received));
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
            return true;
        }

        // Discard the whole invalid line so its suffix cannot become a command.
        if (command.size() == record_capacity - 1 || byte == '\0') {
            command.clear();
            discard_command = true;
            continue;
        }

        command += byte;
    }
}

// Run one external query and wait for its output. Keep the visitor's socket
// open so serve_player can handle the next command after this function returns.
auto send_animal_info(const int32_t client_socket, const std::string& animal)
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
        execl(
            "./zoo_info",
            "zoo_info",
            animal.c_str(),
            static_cast<char*>(nullptr)
        );
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

[[nodiscard]] auto send_area(const int32_t x, const int32_t y) -> std::string {
    auto area{std::string{}};

    for (auto dy{int32_t{-1}}; dy <= 1; dy++) {
        for (auto dx{int32_t{-1}}; dx <= 1; dx++) {
            const auto nx{x + dx};
            const auto ny{y + dy};
            // Zero marks a cell outside the map.
            const auto value{
                nx >= 0 && nx < map_size && ny >= 0 && ny < map_size
                    ? zoo_map[ny][nx]
                    : uint8_t{0}
            };

            if (!area.empty()) {
                area += ',';
            }

            area += std::to_string(value);
        }
    }
    return area;
}

// Run in a client thread. Local session state belongs to this visitor;
// the map and open sockets are shared by all threads.
auto serve_player(void* const argument) -> void* {
    const auto* const socket_argument{static_cast<const int32_t*>(argument)};
    const auto client_socket{*socket_argument};
    // The socket number is copied, so the heap argument is no longer needed.
    delete socket_argument;

    clients.emplace_back(client_socket);
    std::printf("[%d] Client connected\n", getpid());
    auto input{InputBuffer{}};
    auto command{std::string{}};
    auto name{std::string{}};
    int32_t x;
    int32_t y;
    auto registered{false};

    while (read_command(client_socket, input, command)) {
        if (debug) {
            std::printf("[%d] Command: %s\n", getpid(), command.c_str());
        }

        const auto arguments{split_words(command)};

        if (registered) {
            if (command == "QUIT") {
                write_all(client_socket, "BYE " + name + "\n");
                break;
            }
            if (command.starts_with("MSG ")) {
                // Preserve the text after the four-byte "MSG " prefix.
                broadcast_message(client_socket, name, command.substr(4));
                // Chat does not change position, so skip the position reply.
                continue;
            }
            if (command.starts_with("INFO ")) {
                if (arguments.size() == 2) {
                    send_animal_info(client_socket, arguments[1]);
                }
                continue;
            }

            // Preserve the original directions. A move at an edge stays put.
            if (command == "MOVE NORTH") {
                if (y < map_size - 1) {
                    ++y;
                }
            } else if (command == "MOVE SOUTH") {
                if (y > 0) {
                    --y;
                }
            } else if (command == "MOVE EAST") {
                if (x < map_size - 1) {
                    ++x;
                }
            } else if (command == "MOVE WEST") {
                if (x > 0) {
                    --x;
                }
            } else if (command != "WHERE" && command != "AREA") {
                continue;
            }
        } else {
            if (!command.starts_with("HELLO ") || arguments.size() != 2) {
                continue;
            }
            name = arguments[1];

            // All threads share the PRNG, which main seeds once before starting
            // them.
            x = std::rand() % map_size;
            y = std::rand() % map_size;
            registered = true;
            write_all(
                client_socket,
                "WELCOME " + name + " (" + std::to_string(x) + ","
                    + std::to_string(y) + ")\n"
            );
        }

        const auto response{
            "POS " + std::to_string(x) + " " + std::to_string(y) + " AREA "
            + send_area(x, y) + "\n"
        };
        std::printf("[%d] %s at (%d,%d)\n", getpid(), name.c_str(), x, y);
        if (debug) {
            std::printf("[%d] Reply: %s", getpid(), response.c_str());
        }
        write_all(client_socket, response);

        if (command.starts_with("HELLO ") || command.starts_with("MOVE ")) {
            // Text notices preserve each recipient's own POS/map state.
            broadcast_message(
                client_socket,
                name,
                "position (" + std::to_string(x) + "," + std::to_string(y) + ")"
            );
        }
    }

    // Remove the socket before closing it, because the OS can reuse its number.
    const auto removed{std::erase(clients, client_socket)};
    assert(removed == 1);
    std::printf("[%d] Client disconnected\n", getpid());
    close(client_socket);
    // Returning from the pthread entry function ends only this thread.
    return nullptr;
}

// The main thread accepts connections; each client gets its own thread.
auto main(const int32_t argc, char** const argv) -> int {
    // Line-buffer logs so each completed line appears immediately.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::srand(static_cast<uint32_t>(getpid()));

    if (argc == 2 && std::string{argv[1]} == "-d") {
        debug = true;
    }

    for (int i = 0; i < map_size; i++) {
        for (int j = 0; j < map_size; j++) {
            zoo_map[i][j] = static_cast<uint8_t>(std::rand() % 9 + 1);
        }
    }

    const auto server_socket{create_server_socket()};

    // Read the keyboard separately so main can keep accepting clients.
    auto console_thread{pthread_t{}};
    const auto console_create_result{
        pthread_create(&console_thread, nullptr, serve_console, nullptr)
    };
    assert(console_create_result == 0);
    const auto console_detach_result{pthread_detach(console_thread)};
    assert(console_detach_result == 0);

    while (true) {
        // accept returns a new socket for one visitor and keeps the listener.
        // nullptr skips the peer's address output, which we do not need.
        const auto client_socket{accept(server_socket, nullptr, nullptr)};
        assert(client_socket != -1);

        auto thread{pthread_t{}};
        // Heap storage keeps the argument alive after this loop advances.
        const auto create_result{pthread_create(
            &thread, nullptr, serve_player, new int32_t{client_socket}
        )};
        assert(create_result == 0);

        // No thread result is needed; detach releases its resources on exit.
        const auto detach_result{pthread_detach(thread)};
        assert(detach_result == 0);

        // Threads share descriptors, so only the client thread closes its
        // socket.
    }
}
