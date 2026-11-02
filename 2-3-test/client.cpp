#include "io.hpp"
#include <arpa/inet.h>
#include <array>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <pthread.h>
#include <random>
#include <unistd.h>

auto baker_worker(void* const argument) -> void* {
    const auto descriptor{*static_cast<const int32_t*>(argument)};
    std::fputs("Pizza count: ", stdout);
    auto count{int32_t{0}};
    const auto count_read{static_cast<bool>(std::cin >> count)};
    assert(count_read && count > 0);

    constexpr auto names{std::array{
        "Margherita", "Prosciutto", "Capricciosa", "Quattro Formaggi", "Diavola"
    }};
    auto random{std::mt19937{std::random_device{}()}};
    auto choose{std::uniform_int_distribution<size_t>{0, names.size() - 1}};
    for (auto sent{int32_t{0}}; sent < count; ++sent) {
        const auto* const pizza{names[choose(random)]};
        const auto sent_pizza{send_line(descriptor, pizza)};
        assert(sent_pizza);
        const auto confirmed{receive_ok(descriptor)};
        assert(confirmed);
        std::printf("Sent: %s\n", pizza);
    }
    return nullptr;
}

auto customer_worker(void* const argument) -> void* {
    const auto descriptor{*static_cast<const int32_t*>(argument)};
    char pizza[pizza_capacity];
    while (read_line(descriptor, pizza)) {
        std::printf("Received: %s\n", pizza);
        const auto confirmed{send_line(descriptor, "OK")};
        assert(confirmed);
    }
    return nullptr;
}

auto main(const int32_t argc, char** const argv) -> int {
    assert(argc == 3 && "Usage: pizzeria_client <IPv4_address> <port>");
    const auto port{std::stoi(argv[2])};
    assert(port > 0 && port <= 65535);
    const auto buffering{std::setvbuf(stdout, nullptr, _IONBF, 0)};
    assert(buffering == 0);

    auto descriptor{socket(AF_INET, SOCK_STREAM, 0)};
    assert(descriptor != -1);
    auto address{sockaddr_in{}};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));
    const auto converted{inet_pton(AF_INET, argv[1], &address.sin_addr)};
    assert(converted == 1);
    const auto connected{connect(
        descriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address)
    )};
    assert(connected == 0);

    char prompt[pizza_capacity];
    const auto prompted{read_line(descriptor, prompt)};
    assert(prompted);
    std::printf("%s [pekar/zakaznik]: ", prompt);
    auto role{std::string{}};
    const auto role_read{static_cast<bool>(std::getline(std::cin, role))};
    assert(role_read && (role == "pekar" || role == "zakaznik"));
    const auto role_sent{send_line(descriptor, role)};
    assert(role_sent);

    auto worker{pthread_t{}};
    const auto created{pthread_create(
        &worker,
        nullptr,
        role == "pekar" ? baker_worker : customer_worker,
        &descriptor
    )};
    assert(created == 0);
    const auto joined{pthread_join(worker, nullptr)};
    assert(joined == 0);
    const auto closed{close(descriptor)};
    assert(closed == 0);
    return 0;
}
