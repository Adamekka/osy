#include "io.hpp"
#include "zoo_map.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// The keyboard updates this state, needs to be global
auto movement_thread{pthread_t{}};
auto movement_running{false};

// This thread receives replies from the server and updates the map
auto receive_messages(void* const argument) -> void* {
    const auto server_socket{*static_cast<const int32_t*>(argument)};
    auto buffer{std::string(1024, '\0')};
    auto pending{std::string{}};

    while (true) {
        const auto received_len{
            read(server_socket, buffer.data(), buffer.size())
        };
        assert(received_len >= 0);
        if (received_len == 0) {
            return nullptr;
        }
        pending.append(buffer, 0, static_cast<size_t>(received_len));
        while (true) {
            const auto newline{pending.find('\n')};
            if (newline == std::string::npos) {
                break;
            }
            const auto line{pending.substr(0, newline)};
            pending.erase(0, newline + 1);
            std::puts(line.c_str());

            const auto fields{split_words(line)};
            const auto is_player{fields.size() == 4 && fields[0] == "PLAYER"};
            if (!is_player
                && (fields.size() < 5 || fields[0] != "POS"
                    || fields[3] != "AREA")) {
                continue;
            }

            // PLAYER includes a name before x/y, POS starts with x/y.
            int32_t x;
            const auto& x_text{fields[is_player ? 2 : 1]};
            const auto x_end{x_text.data() + x_text.size()};
            const auto x_result{std::from_chars(x_text.data(), x_end, x)};
            if (x_result.ec != std::errc{} || x_result.ptr != x_end) {
                continue;
            }

            int32_t y;
            const auto& y_text{fields[is_player ? 3 : 2]};
            const auto y_end{y_text.data() + y_text.size()};
            const auto y_result{std::from_chars(y_text.data(), y_end, y)};
            if (y_result.ec != std::errc{} || y_result.ptr != y_end) {
                continue;
            }

            if (is_player) {
                update_other_player(fields[1].c_str(), x, y);
            } else {
                update_map(x, y, fields[4].c_str());
            }
            draw_map();
        }
    }
}

auto move_randomly(void* const argument) -> void* {
    const auto server_socket{*static_cast<const int32_t*>(argument)};
    const std::string moves[]{
        "MOVE NORTH\n", "MOVE SOUTH\n", "MOVE WEST\n", "MOVE EAST\n"
    };

    while (true) {
        sleep(5);
        write_all(server_socket, moves[std::rand() % 4]);
    }
}

auto stop_movement() -> void {
    if (!movement_running) {
        return;
    }

    const auto cancelled{pthread_cancel(movement_thread)};
    assert(cancelled == 0);
    const auto joined{pthread_join(movement_thread, nullptr)};
    assert(joined == 0);
    movement_running = false;
}

// This thread reads the keyboard and sends commands to the server
auto send_commands(void* const argument) -> void* {
    const auto server_socket{*static_cast<const int32_t*>(argument)};
    auto command{std::string{}};

    while (std::getline(std::cin, command)) {
        if (command == "START") {
            if (!movement_running) {
                const auto created{pthread_create(
                    &movement_thread, nullptr, move_randomly, argument
                )};
                assert(created == 0);
                movement_running = true;
            }
            continue;
        }
        if (command == "STOP") {
            stop_movement();
            continue;
        }

        if (command == "w" || command == "W") {
            command = "MOVE NORTH";
        } else if (command == "s" || command == "S") {
            command = "MOVE SOUTH";
        } else if (command == "a" || command == "A") {
            command = "MOVE WEST";
        } else if (command == "d" || command == "D") {
            command = "MOVE EAST";
        }

        // getline removes '\n', add it back to the end
        write_all(server_socket, command + "\n");
    }
    assert(std::cin.eof());

    stop_movement();
    const auto shutdown_result{shutdown(server_socket, SHUT_WR)};
    assert(shutdown_result == 0);
    return nullptr;
}

// Prompt for the name, then forward commands and draw received map areas.
auto main(const int32_t argc, char** const argv) -> int {
    assert(argc == 3 && "Usage: zoo_client <IPv4_address> <port>");

    map_init(map_size, map_size);

    char* port_end;
    const auto port{std::strtol(argv[2], &port_end, 10)};
    // Reject trailing text as well as ports outside the TCP port range.
    assert(*port_end == '\0' && port > 0 && port <= 65535);

    // pthread_create takes void*, so this argument storage is non-const.
    auto server_socket{socket(AF_INET, SOCK_STREAM, 0)};
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
    auto name{std::string{}};
    const auto name_read{static_cast<bool>(std::getline(std::cin, name))};
    assert(name_read);
    if (!name.empty() && name.back() == '\r') {
        name.pop_back();
    }
    write_all(server_socket, "HELLO " + name + "\n");

    std::srand(static_cast<uint32_t>(getpid()));

    // Start GET SERVER_DATA
    auto receiver{pthread_t{}};
    const auto receiver_created{
        pthread_create(&receiver, nullptr, receive_messages, &server_socket)
    };
    assert(receiver_created == 0);

    // Start GET KEYBOARD, SET SERVER_DATA
    auto keyboard{pthread_t{}};
    const auto keyboard_created{
        pthread_create(&keyboard, nullptr, send_commands, &server_socket)
    };
    assert(keyboard_created == 0);

    // Wait for server to shut down
    const auto receiver_joined{pthread_join(receiver, nullptr)};
    assert(receiver_joined == 0);

    // Tell keyboard to shut down
    const auto keyboard_cancelled{pthread_cancel(keyboard)};
    assert(keyboard_cancelled == 0);

    // Wait for keyboard to shut down
    const auto keyboard_joined{pthread_join(keyboard, nullptr)};
    assert(keyboard_joined == 0);

    stop_movement();
    close(server_socket);
    return 0;
}
