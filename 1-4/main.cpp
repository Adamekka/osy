#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
    const auto message_count{std::atoi(argv[2])};

    // Make each process's log visible before it forwards the message.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    int32_t pipe12[2];
    int32_t pipe23[2];
    int32_t pipe3parent[2];

    pipe(pipe12);
    pipe(pipe23);
    pipe(pipe3parent);

    const auto child1{fork()};

    if (child1 == 0) {
        close(pipe12[FD_READ]);
        close(pipe23[FD_READ]);
        close(pipe23[FD_WRITE]);
        close(pipe3parent[FD_READ]);
        close(pipe3parent[FD_WRITE]);

        auto* const input{std::fopen(file, "r")};
        constexpr auto max_lines{size_t{1024}};

        // Keep the 4 MiB message table off the stack.
        static char messages[max_lines][RECORD_CAPACITY];

        char line[RECORD_CAPACITY];
        auto loaded_count{size_t{0}};

        while (std::fgets(line, sizeof(line), input) != nullptr) {
            auto* const newline{std::strchr(line, '\n')};

            if (newline != nullptr) {
                *newline = '\0';
            }

            std::strcpy(messages[loaded_count], line);
            ++loaded_count;
        }

        std::fclose(input);

        // Seed once so each run varies its random selections.
        std::srand(static_cast<uint32_t>(getpid()));

        for (auto sent{int32_t{0}}; sent < message_count; ++sent) {
            const auto index{std::rand() % loaded_count};

            char record[RECORD_CAPACITY];
            std::snprintf(
                record, sizeof(record), "%zu\t%s\n", index + 1, messages[index]
            );

            std::printf(
                "[PID %d] Selected: \"%s\" (line %zu)\n",
                getpid(),
                messages[index],
                index + 1
            );
            write_record(pipe12[FD_WRITE], record);
        }

        close(pipe12[FD_WRITE]);
        return 0;
    }

    const auto child2{fork()};

    if (child2 == 0) {
        close(pipe12[FD_WRITE]);
        close(pipe23[FD_READ]);
        close(pipe3parent[FD_READ]);
        close(pipe3parent[FD_WRITE]);

        char record[RECORD_CAPACITY];
        auto sequence{size_t{1}};

        while (read_record(pipe12[FD_READ], record)) {
            char numbered[RECORD_CAPACITY];
            std::snprintf(
                numbered, sizeof(numbered), "%zu\t%s\n", sequence, record
            );

            const auto original_line{std::atoi(record)};
            const auto* const text{std::strchr(record, '\t') + 1};
            std::printf(
                "[PID %d] Numbered: %zu. %s (line %d)\n",
                getpid(),
                sequence,
                text,
                original_line
            );
            write_record(pipe23[FD_WRITE], numbered);
            ++sequence;
        }

        close(pipe12[FD_READ]);
        close(pipe23[FD_WRITE]);
        return 0;
    }

    const auto child3{fork()};

    if (child3 == 0) {
        close(pipe12[FD_READ]);
        close(pipe12[FD_WRITE]);
        close(pipe23[FD_WRITE]);
        close(pipe3parent[FD_READ]);

        char record[RECORD_CAPACITY];

        while (read_record(pipe23[FD_READ], record)) {
            const auto* const original_line_field{
                std::strchr(record, '\t') + 1
            };
            const auto* const text{std::strchr(original_line_field, '\t') + 1};

            // atoi stops at the tab, leaving the text fields intact.
            const auto sequence{std::atoi(record)};
            const auto original_line{std::atoi(original_line_field)};

            auto length{size_t{0}};

            // UTF-8 continuation bytes belong to the preceding character.
            for (const auto* byte{text}; *byte != '\0'; ++byte) {
                if ((static_cast<uint8_t>(*byte) & 0xC0) != 0x80) {
                    ++length;
                }
            }

            char analyzed[RECORD_CAPACITY];
            std::snprintf(
                analyzed,
                sizeof(analyzed),
                "%d. %s (%zu) - line %d\n",
                sequence,
                text,
                length,
                original_line
            );

            std::printf("[PID %d] Analyzed: %s", getpid(), analyzed);
            write_record(pipe3parent[FD_WRITE], analyzed);
        }

        close(pipe23[FD_READ]);
        close(pipe3parent[FD_WRITE]);
        return 0;
    }

    close(pipe12[FD_READ]);
    close(pipe12[FD_WRITE]);
    close(pipe23[FD_READ]);
    close(pipe23[FD_WRITE]);
    close(pipe3parent[FD_WRITE]);

    char record[RECORD_CAPACITY];

    // Drain the pipe before waiting so a full pipe cannot block the children.
    while (read_record(pipe3parent[FD_READ], record)) {
        std::printf("[PID %d] Received: %s\n", getpid(), record);
    }

    close(pipe3parent[FD_READ]);

    for (auto reaped{int32_t{0}}; reaped < 3; ++reaped) {
        int32_t status;
        wait(&status);
    }

    return 0;
}
