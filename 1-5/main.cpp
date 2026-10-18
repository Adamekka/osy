#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

#define FD_READ 0
#define FD_WRITE 1

constexpr auto RECORD_CAPACITY{size_t{4096}};

[[nodiscard]] auto
read_record(const int32_t fd, char (&record)[RECORD_CAPACITY]) -> bool {
    auto used{size_t{0}};
    char byte;

    // Stop at a newline so the next call starts at the next record.
    while (read(fd, &byte, sizeof(byte)) > 0) {
        if (byte == '\n') {
            record[used] = '\0';
            return true;
        }

        record[used] = byte;
        ++used;
    }

    return false;
}

auto write_record(const int32_t fd, const char* const record) -> void {
    const auto length{std::strlen(record)};
    auto offset{size_t{0}};

    // A successful write may send only part of the record.
    while (offset < length) {
        const auto written{write(fd, record + offset, length - offset)};
        offset += static_cast<size_t>(written);
    }
}

auto main(int argc, char* argv[]) -> int {
    const auto* const file{argv[1]};

    // Make each process's log visible before it forwards the message.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    int32_t pipe12[2];
    int32_t pipe23[2];
    int32_t pipe34[2];

    pipe(pipe12);
    pipe(pipe23);
    pipe(pipe34);

    const auto child1{fork()};

    if (child1 == 0) {
        close(pipe12[FD_READ]);
        close(pipe23[FD_READ]);
        close(pipe23[FD_WRITE]);
        close(pipe34[FD_READ]);
        close(pipe34[FD_WRITE]);

        auto* const input{std::fopen(file, "r")};
        char line[RECORD_CAPACITY];

        while (std::fgets(line, sizeof(line), input) != nullptr) {
            // Keep the newline so sort can distinguish the input lines.
            write_record(pipe12[FD_WRITE], line);
        }

        std::fclose(input);
        close(pipe12[FD_WRITE]);
        return 0;
    }

    const auto child2{fork()};

    if (child2 == 0) {
        dup2(pipe12[FD_READ], STDIN_FILENO);
        dup2(pipe23[FD_WRITE], STDOUT_FILENO);

        // stdin and stdout now hold the needed pipe connections.
        close(pipe12[FD_READ]);
        close(pipe12[FD_WRITE]);
        close(pipe23[FD_READ]);
        close(pipe23[FD_WRITE]);
        close(pipe34[FD_READ]);
        close(pipe34[FD_WRITE]);

        char command[]{"sort"};
        char* const arguments[]{command, nullptr};

        execvp(command, arguments);

        // execvp returns only on failure; stop before reaching later forks.
        return 1;
    }

    const auto child3{fork()};

    if (child3 == 0) {
        dup2(pipe23[FD_READ], STDIN_FILENO);
        dup2(pipe34[FD_WRITE], STDOUT_FILENO);

        // stdin and stdout now hold the needed pipe connections.
        close(pipe12[FD_READ]);
        close(pipe12[FD_WRITE]);
        close(pipe23[FD_READ]);
        close(pipe23[FD_WRITE]);
        close(pipe34[FD_READ]);
        close(pipe34[FD_WRITE]);

        char command[]{"tr"};
        char lowercase[]{"a-z"};
        char uppercase[]{"A-Z"};
        char* const arguments[]{command, lowercase, uppercase, nullptr};

        execvp(command, arguments);

        // execvp returns only on failure; stop before reaching later forks.
        return 1;
    }

    const auto child4{fork()};

    if (child4 == 0) {
        close(pipe12[FD_READ]);
        close(pipe12[FD_WRITE]);
        close(pipe23[FD_READ]);
        close(pipe23[FD_WRITE]);
        close(pipe34[FD_WRITE]);

        char record[RECORD_CAPACITY];
        auto message_count{size_t{0}};
        auto total_length{size_t{0}};

        while (read_record(pipe34[FD_READ], record)) {
            ++message_count;

            // Count UTF-8 characters so accented letters count once.
            for (const auto* byte{record}; *byte != '\0'; ++byte) {
                if ((static_cast<uint8_t>(*byte) & 0xC0) != 0x80) {
                    ++total_length;
                }
            }
        }

        // The assignment's input is assumed to contain at least one message.
        const auto average{static_cast<double>(total_length) / message_count};

        std::printf(
            "Chat 4: received %zu messages, average length %.2f characters\n",
            message_count,
            average
        );
    }

    close(pipe12[FD_READ]);
    close(pipe12[FD_WRITE]);
    close(pipe23[FD_READ]);
    close(pipe23[FD_WRITE]);
    close(pipe34[FD_READ]);
    close(pipe34[FD_WRITE]);

    for (auto reaped{int32_t{0}}; reaped < 4; ++reaped) {
        int32_t status;
        wait(&status);
    }

    return 0;
}
