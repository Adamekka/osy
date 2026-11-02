#include "io.hpp"
#include <cassert>
#include <cstdio>
#include <netinet/in.h>
#include <pthread.h>
#include <queue>
#include <semaphore.h>
#include <unistd.h>

constexpr auto buffer_capacity{uint32_t{10}};
auto buffer{std::queue<std::string>{}};
auto mutex{sem_t{}};
auto empty{sem_t{}};
auto full{sem_t{}};

auto down(sem_t& semaphore) -> void {
    auto result{sem_wait(&semaphore)};
    while (result == -1 && errno == EINTR) {
        result = sem_wait(&semaphore);
    }
    assert(result == 0);
}

auto up(sem_t& semaphore) -> void {
    const auto result{sem_post(&semaphore)};
    assert(result == 0);
}

auto producer(char* const pizza) -> void {
    assert(pizza != nullptr && pizza[0] != '\0');
    assert(std::strlen(pizza) < pizza_capacity);
    down(empty);
    down(mutex);
    assert(buffer.size() < buffer_capacity);
    buffer.emplace(pizza);
    std::printf(
        "Stored: %s | stock: %zu/%zu\n",
        pizza,
        buffer.size(),
        static_cast<size_t>(buffer_capacity)
    );
    up(mutex);
    up(full);
}

auto consumer(char* const pizza_out) -> void {
    assert(pizza_out != nullptr);
    down(full);
    down(mutex);
    assert(!buffer.empty());
    std::strcpy(pizza_out, buffer.front().c_str());
    buffer.pop();
    std::printf(
        "Taken: %s | stock: %zu/%zu\n",
        pizza_out,
        buffer.size(),
        static_cast<size_t>(buffer_capacity)
    );
    up(mutex);
    up(empty);
}

auto serve_client_worker(void* const argument) -> void* {
    const auto* const descriptor_argument{
        static_cast<const int32_t*>(argument)
    };
    const auto descriptor{*descriptor_argument};
    delete descriptor_argument;
    char role[pizza_capacity];
    char pizza[pizza_capacity];
    if (send_line(descriptor, "Role?") && read_line(descriptor, role)) {
        if (std::strcmp(role, "pekar") == 0) {
            while (read_line(descriptor, pizza) && pizza[0] != '\0') {
                producer(pizza);
                if (!send_line(descriptor, "OK")) {
                    break;
                }
            }
        } else if (std::strcmp(role, "zakaznik") == 0) {
            while (true) {
                consumer(pizza);
                if (!send_line(descriptor, pizza) || !receive_ok(descriptor)) {
                    break;
                }
            }
        }
    }
    const auto closed{close(descriptor)};
    assert(closed == 0);
    return nullptr;
}

auto main(const int32_t argc, char** const argv) -> int {
    assert(argc == 2 && "Usage: pizzeria <port>");
    const auto port{std::stoi(argv[1])};
    assert(port > 0 && port <= 65535);

    // Docker captures stdout through a pipe; keep stock changes visible.
    const auto buffering{std::setvbuf(stdout, nullptr, _IONBF, 0)};
    assert(buffering == 0);

    const auto mutex_result{sem_init(&mutex, 0, 1)};
    assert(mutex_result == 0);
    const auto empty_result{sem_init(&empty, 0, buffer_capacity)};
    assert(empty_result == 0);
    const auto full_result{sem_init(&full, 0, 0)};
    assert(full_result == 0);

    const auto listener{socket(AF_INET, SOCK_STREAM, 0)};
    assert(listener != -1);
    const auto reuse{int32_t{1}};
    const auto reusable{
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))
    };
    assert(reusable == 0);

    auto address{sockaddr_in{}};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));
    // Listen on container interfaces so Docker's published port can reach us.
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    const auto bound{bind(
        listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)
    )};
    assert(bound == 0);
    const auto listening{listen(listener, SOMAXCONN)};
    assert(listening == 0);
    std::printf("Pizzeria listening on port %u\n", static_cast<uint32_t>(port));

    while (true) {
        auto descriptor{accept(listener, nullptr, nullptr)};
        while (descriptor == -1 && errno == EINTR) {
            descriptor = accept(listener, nullptr, nullptr);
        }
        if (descriptor == -1 && errno == ECONNABORTED) {
            continue;
        }
        assert(descriptor != -1);
        auto worker{pthread_t{}};
        const auto created{pthread_create(
            &worker, nullptr, serve_client_worker, new int32_t{descriptor}
        )};
        assert(created == 0);
        const auto detached{pthread_detach(worker)};
        assert(detached == 0);
    }
}
