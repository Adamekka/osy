#include <cassert>
#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <pthread.h>
#include <queue>
#include <semaphore.h>
#include <thread>

#ifdef NDEBUG
    #error "Build this exercise with assertions enabled."
#endif

constexpr auto buffer_capacity{uint32_t{10}};
auto buffer{std::queue<uint64_t>{}};
auto mutex{sem_t{}};
auto empty{sem_t{}};
auto full{sem_t{}};

// mutex = 1
// empty = 10
// full = 0

// Producer
// wait(empty)    -> reserve space; empty 10 -> 9
// wait(mutex)    -> acquire exclusive buffer access; mutex 1 -> 0
// insert item
// post(mutex)    -> release buffer access; mutex 0 -> 1
// post(full)     -> announce an available item; full 0 -> 1

// Consumer
// wait(full)     -> reserve an item; full 1 -> 0
// wait(mutex)    -> acquire exclusive buffer access; mutex 1 -> 0
// remove item
// post(mutex)    -> release buffer access; mutex 0 -> 1
// post(empty)    -> announce more available space; empty 9 -> 10

auto down(sem_t& semaphore) -> void {
    auto result{sem_wait(&semaphore)};
    // A signal can interrupt a wait without granting a permit; retry that wait.
    while (result == -1 && errno == EINTR) {
        result = sem_wait(&semaphore);
    }
    assert(result == 0);
}

auto up(sem_t& semaphore) -> void {
    const auto result{sem_post(&semaphore)};
    assert(result == 0);
}

auto producer([[maybe_unused]] void* const argument) -> void* {
    auto item{uint64_t{0}};
    while (true) {
        // Simulate production outside the lock so the consumer can keep
        // working.
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        ++item;

        down(empty);
        down(mutex);
        assert(buffer.size() < buffer_capacity);
        buffer.emplace(item);
        // Log under the same lock so each reported size matches that operation.
        std::printf(
            "Produced %" PRIu64 " | buffer: %zu/%zu\n",
            item,
            buffer.size(),
            static_cast<size_t>(buffer_capacity)
        );
        up(mutex);
        up(full);
    }
}

auto consumer([[maybe_unused]] void* const argument) -> void* {
    while (true) {
        down(full);
        down(mutex);
        assert(!buffer.empty());
        const auto item{buffer.front()};
        buffer.pop();
        std::printf(
            "Consumed %" PRIu64 " | buffer: %zu/%zu\n",
            item,
            buffer.size(),
            static_cast<size_t>(buffer_capacity)
        );
        up(mutex);
        up(empty);

        // Slower consumption makes the full buffer and producer blocking
        // visible.
        std::this_thread::sleep_for(std::chrono::milliseconds{300});
    }
}

auto main() -> int {
    // Docker may capture stdout through a pipe; show each transfer immediately.
    const auto buffering_result{std::setvbuf(stdout, nullptr, _IONBF, 0)};
    assert(buffering_result == 0);

    // Zero means these unnamed semaphores are shared between threads, not
    // processes.
    const auto mutex_result{sem_init(&mutex, 0, 1)};
    assert(mutex_result == 0);
    const auto empty_result{sem_init(&empty, 0, buffer_capacity)};
    assert(empty_result == 0);
    const auto full_result{sem_init(&full, 0, 0)};
    assert(full_result == 0);

    auto producer_thread{pthread_t{}};
    auto consumer_thread{pthread_t{}};
    const auto consumer_result{
        pthread_create(&consumer_thread, nullptr, consumer, nullptr)
    };
    assert(consumer_result == 0);
    const auto producer_result{
        pthread_create(&producer_thread, nullptr, producer, nullptr)
    };
    assert(producer_result == 0);

    // Both workers loop forever, as in the book. Main waits until Ctrl+C stops
    // the process, which also releases these unnamed semaphore resources.
    const auto producer_join_result{pthread_join(producer_thread, nullptr)};
    assert(producer_join_result == 0);
    const auto consumer_join_result{pthread_join(consumer_thread, nullptr)};
    assert(consumer_join_result == 0);
    return 0;
}
