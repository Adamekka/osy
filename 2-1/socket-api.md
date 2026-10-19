# How to use sockets, poll, and processes

This tutorial shows how to call the functions in small C++ examples. Start by connecting two programs, then send bytes, wait for input, and run another program with redirected output.

## Set up the examples

Use these headers:

```cpp
#include <arpa/inet.h>
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
```

Put the example fragments inside `main`:

```cpp
auto main() -> int {
    // Insert the example you want to try here.
    return 0;
}
```

The server and client examples go in separate programs. Later examples use `peer_socket` to mean the connected socket obtained by either program. Append communication fragments after connection setup, before returning from `main`. The `write_all` function introduced below goes above `main`.

Compile an example with:

```sh
clang++ -std=c++20 -Wall -Wextra -Wpedantic example.cpp -o example
```

The examples use assertions for system-call failures. Keep assertions enabled while trying them. Calls are made before the assertions, so disabling assertions does not remove the calls themselves.

## 1. Accept a TCP connection

Use this sequence in the server:

```text
socket -> bind -> listen -> accept
```

```cpp
const auto listener{socket(AF_INET, SOCK_STREAM, 0)};
assert(listener != -1);

auto address{sockaddr_in{}};
address.sin_family = AF_INET;
address.sin_port = htons(12345);
address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

const auto bound{bind(
    listener,
    reinterpret_cast<const sockaddr*>(&address),
    sizeof(address)
)};
assert(bound == 0);

const auto listening{listen(listener, SOMAXCONN)};
assert(listening == 0);

const auto peer_socket{accept(listener, nullptr, nullptr)};
assert(peer_socket != -1);
```

`socket(AF_INET, SOCK_STREAM, 0)` creates an IPv4 TCP socket. Its return value is a file descriptor, a nonnegative integer used in later calls. `-1` means failure.

Fill a `sockaddr_in` with the address you want to listen on:

- `sin_family = AF_INET` selects IPv4.
- `htons(12345)` converts port `12345` into network byte order.
- `htonl(INADDR_LOOPBACK)` selects the local machine, `127.0.0.1`. Use `htonl(INADDR_ANY)` to accept connections on all local IPv4 interfaces.

`bind` assigns that local address to the socket. It takes a generic `sockaddr` pointer, so the IPv4 address pointer is cast to that type. Its third argument is the size of the address structure.

Choose an unused port and use the same number in both programs. If another server already uses `12345`, choose a different port for these examples.

`listen` enables incoming connections. The backlog argument controls the pending-connection queue; it is not the total number of clients your program can serve.

`accept` waits for one connection and returns a new socket for talking to that client. The listener remains available for future `accept` calls. The two `nullptr` arguments mean you do not need the client's address returned.

At this point, leave the server running in `accept` and start the client example below in another terminal.

## 2. Connect to the server

Use this sequence in the client:

```text
socket -> connect
```

```cpp
const auto peer_socket{socket(AF_INET, SOCK_STREAM, 0)};
assert(peer_socket != -1);

auto address{sockaddr_in{}};
address.sin_family = AF_INET;
address.sin_port = htons(12345);
const auto converted{
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr)
};
assert(converted == 1);

const auto connected{connect(
    peer_socket,
    reinterpret_cast<const sockaddr*>(&address),
    sizeof(address)
)};
assert(connected == 0);
```

`inet_pton` converts a textual IP address into the binary address field. It returns `1` for a valid address, `0` for invalid text, or `-1` for an error. It already produces network-order bytes; do not apply `htonl` to its result.

`connect` takes the server's address and establishes the connection. Success returns `0`. Afterward, both programs can use `read` and `write` on their connected `peer_socket`.

## 3. Send bytes with write

`write(fd, data, count)` tries to write `count` bytes and returns how many it actually wrote. That number can be smaller than requested, so advance through the data until everything has been sent.

Put this reusable function above `main`:

```cpp
auto write_all(
    const int32_t descriptor, const char* const data, const size_t size
) -> void {
    auto sent{size_t{0}};
    while (sent < size) {
        const auto written{write(descriptor, data + sent, size - sent)};
        assert(written > 0);
        sent += static_cast<size_t>(written);
    }
}
```

Then send a C string from either connected program:

```cpp
const char* const message{"PING\n"};
write_all(peer_socket, message, std::strlen(message));
```

`data + sent` points at the next unsent byte. `size - sent` is the number left to send. Only convert `written` to `size_t` after checking it, because a negative error value would otherwise become a large unsigned value.

`strlen` excludes the terminating `\0`. The newline is included because it is part of the string. This gives the receiver a delimiter it can use to recognize a complete line.

You can use the same helper with `STDOUT_FILENO` to display a known number of bytes on the terminal.

## 4. Read bytes and turn them into a C string

In the other connected program, read the message:

```cpp
char buffer[1024];
// Leave one byte for a terminator because we will use the data as a C string.
const auto received{read(peer_socket, buffer, sizeof(buffer) - 1)};
assert(received >= 0);

if (received == 0) {
    std::puts("Peer disconnected");
} else {
    buffer[static_cast<size_t>(received)] = '\0';
    std::printf("%s", buffer);
}
```

`read` returns a positive byte count, `0` at EOF, or `-1` on failure. It does not append `\0`, and it does not wait for the entire buffer to fill.

Here the buffer needs a terminator because `printf("%s", ...)` expects a C string. If you only want to forward the bytes, send the returned byte count with `write_all` and skip string conversion.

A successful read does not necessarily contain one complete command. TCP may deliver `"PI"` followed by `"NG\n"`, or several lines in one read. To parse a line protocol:

1. Append received bytes to your input buffer.
2. Extract a command only when a newline is present.
3. Keep bytes after that newline for the next command.

Try sending two lines in one write, then sending one line in two writes. Your command parser should produce the same commands in both cases.

## 5. Parse a command and format a reply

For a command with one word argument, combine a prefix check with a bounded conversion:

```cpp
const char* const command{"SAY hello"};
char word[64];
char extra;
const auto matched{std::sscanf(command, "SAY %63s %c", word, &extra)};

if (std::strncmp(command, "SAY ", 4) == 0 && matched == 1) {
    char reply[128];
    const auto length{
        std::snprintf(reply, sizeof(reply), "OK %s\n", word)
    };
    assert(length > 0 && static_cast<size_t>(length) < sizeof(reply));
    write_all(peer_socket, reply, static_cast<size_t>(length));
}
```

`strncmp` compares the first four bytes, including the space after `SAY`. Use `strcmp(command, "WHERE") == 0` when you want an exact whole-string match. Both functions return `0` when the compared text matches.

`%63s` reads at most `63` non-whitespace bytes and appends `\0`, fitting `word[64]`. The space before `%c` skips whitespace. If an extra argument exists, `%c` reads a character from it and `sscanf` returns `2`. A return value of `1` means exactly the expected word was assigned.

`snprintf` formats the reply into the buffer. Its return value excludes `\0`; a value at least as large as the buffer capacity means the result was truncated. Send the resulting text length, not the size of the whole array.

Try changing the input to `"SAY hello extra"`. The branch should be skipped, without asserting or closing the connection.

## 6. Wait for keyboard input and socket data with poll

Use `poll` when blocking on one input would prevent you from reading another. This example relays keyboard bytes to a connected peer and peer bytes to stdout:

```cpp
pollfd events[]{
    {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0},
    {.fd = peer_socket, .events = POLLIN, .revents = 0}
};
char buffer[1024];

while (true) {
    const auto ready{poll(events, 2, 5000)};
    assert(ready >= 0);
    if (ready == 0) {
        std::puts("No input for five seconds");
        continue;
    }

    if (events[1].revents & (POLLIN | POLLHUP | POLLERR)) {
        const auto received{read(peer_socket, buffer, sizeof(buffer))};
        assert(received >= 0);
        if (received == 0) {
            break;
        }
        write_all(STDOUT_FILENO, buffer, static_cast<size_t>(received));
    }

    if (events[0].revents & (POLLIN | POLLHUP | POLLERR)) {
        const auto typed{read(STDIN_FILENO, buffer, sizeof(buffer))};
        assert(typed >= 0);
        if (typed == 0) {
            // Finish sending, but keep receiving the peer's final replies.
            const auto stopped{shutdown(peer_socket, SHUT_WR)};
            assert(stopped == 0);
            events[0].fd = -1;
        } else {
            write_all(peer_socket, buffer, static_cast<size_t>(typed));
        }
    }
}
```

For `poll(events, 2, 5000)`, the arguments are the array, the number of entries, and the timeout in milliseconds. Use `-1` to wait indefinitely or `0` to check without waiting.

| Field     | How you use it                                                     |
| --------- | ------------------------------------------------------------------ |
| `fd`      | Set the descriptor to watch. A negative value disables that entry. |
| `events`  | Set requested events, such as `POLLIN` for readable input.         |
| `revents` | Inspect the flags written by `poll` after it returns.              |

`events[0]` is the stdin entry and `events[1]` is the socket entry. These are array indices; the actual descriptor is in each entry's `fd` field.

The return value is the number of entries with returned events, `0` on timeout, or `-1` on failure. Readiness says you can attempt I/O; the bytes themselves still come from `read`.

Use bitwise `&` to test a flag because flags can be combined. `POLLHUP` indicates a hangup, but unread bytes can remain, so read before treating a zero-byte result as EOF. `POLLERR` reports an error; a subsequent read may fail.

The five-second interval starts again on each call, so either keyboard input or a peer reply restarts this wait. For an automatic action, replace the timeout message with the action you want to perform.

## 7. Create a process with fork and collect it with waitpid

```cpp
const auto child{fork()};
assert(child != -1);

if (child == 0) {
    std::puts("This is the child");
    // This fragment is inside main; return ends the child here.
    return 0;
}

int32_t status;
const auto waited{waitpid(child, &status, 0)};
assert(waited == child);
if (WIFEXITED(status)) {
    std::printf("Child exit code: %d\n", WEXITSTATUS(status));
}
```

After `fork`, both processes continue after the same call:

- The child gets `0` as the return value.
- The parent gets the child's PID.
- `-1` means no child was created.

Ordinary variables are copied. Changing a variable in the child does not change the parent's copy. Open descriptors are copied too, and refer to the same underlying open sockets, files, or pipes.

`waitpid(child, &status, 0)` waits for that particular child to exit and collects it. Its return value is the child's PID. Read the exit code from `status` only after checking `WIFEXITED(status)`.

To serve one TCP connection per process, put `accept` and `fork` in a loop. In the child, close the listener, handle the connected socket, close it, and exit. In the parent, close its copy of that connected socket and return to `accept`.

Keep the default `SIGCHLD` behavior when using `waitpid`. If you explicitly set `SIGCHLD` to `SIG_IGN` for automatic child collection, restore `SIG_DFL` in a process that needs to wait for its own children. A long-running parent must collect exited children or arrange automatic collection to avoid zombies.

## 8. Run a program with execl and redirect it with dup2

Suppose you want `/bin/echo hello` to write directly to your connected socket:

```cpp
const auto child{fork()};
assert(child != -1);

if (child == 0) {
    const auto redirected{dup2(peer_socket, STDOUT_FILENO)};
    assert(redirected == STDOUT_FILENO);
    // stdout now keeps the socket open through its own descriptor.
    close(peer_socket);

    execl("/bin/echo", "echo", "hello", static_cast<char*>(nullptr));
    // Only failure returns. Stop this child instead of resuming parent code.
    assert(false && "execl failed");
    _exit(EXIT_FAILURE);
}

const auto waited{waitpid(child, nullptr, 0)};
assert(waited == child);
```

Call `dup2(source, destination)` in that order. It makes the destination descriptor refer to the same open object as the source. Here destination `1`, `STDOUT_FILENO`, becomes the socket. The parent's stdout is unchanged because the redirect happens in the child.

`execl` takes an executable path, followed by the new program's argument strings and a null pointer:

| Argument             | Value in this example                    |
| -------------------- | ---------------------------------------- |
| Executable path      | `/bin/echo`                              |
| New `argv[0]`        | `echo`, conventionally the program name. |
| New `argv[1]`        | `hello`                                  |
| End of argument list | `static_cast<char*>(nullptr)`            |

The new program runs in the same child process, with the same PID and redirected stdout. Successful `exec` never returns. It creates no additional process; `fork` created the child beforehand.

Use `execlp("echo", "echo", "hello", static_cast<char*>(nullptr))` when you want to search `PATH`. Use `execl` with an explicit path when you know the executable's location. For a relative path such as `./tool`, the process's working directory determines where it is found.

After a failed `exec`, `_exit` stops the child without flushing copied stdio buffers. If you printed through buffered stdio before a fork, flush those buffers before forking so two processes do not later flush copies of the same output.

## 9. Capture output in a pipe

Use a pipe when the parent needs to read the output itself, for example to inspect it or forward it somewhere else. Create the pipe before forking so both processes inherit its ends.

```cpp
int32_t channel[2];
const auto created{pipe(channel)};
assert(created == 0);

const auto child{fork()};
assert(child != -1);
if (child == 0) {
    close(channel[0]); // The child only writes; it does not read this pipe.
    const auto redirected{dup2(channel[1], STDOUT_FILENO)};
    assert(redirected == STDOUT_FILENO);
    close(channel[1]);

    execl("/bin/echo", "echo", "hello", static_cast<char*>(nullptr));
    assert(false && "execl failed");
    _exit(EXIT_FAILURE);
}

// Closing our writer allows read() to reach EOF when the child finishes.
close(channel[1]);
char buffer[1024];
while (true) {
    const auto received{read(channel[0], buffer, sizeof(buffer))};
    assert(received >= 0);
    if (received == 0) {
        break;
    }
    write_all(STDOUT_FILENO, buffer, static_cast<size_t>(received));
}
close(channel[0]);

const auto waited{waitpid(child, nullptr, 0)};
assert(waited == child);
```

`channel[0]` is the read end and `channel[1]` is the write end. Bytes written to one end are read from the other.

Close unused ends in both processes. If the parent keeps its write end open, its read loop cannot reach EOF just because the child exited.

Drain the pipe before waiting. Otherwise, a child producing enough output could block on a full pipe while the parent waits for that child to finish.

The example displays the captured bytes with `write_all(STDOUT_FILENO, ...)`. Change that destination to a connected socket to forward the captured output over TCP.

## 10. Finish a connection

When you are done reading and writing:

```cpp
close(peer_socket);
```

Close the listener too when the server is shutting down. After `fork`, close each process's unused descriptor copies; closing one copy does not release copies held by other processes.

If you have finished sending but still need a reply, end only the sending direction:

```cpp
const auto stopped{shutdown(peer_socket, SHUT_WR)};
assert(stopped == 0);
```

Then continue reading until EOF and close the descriptor afterward. `shutdown` changes the socket's communication direction, while `close` releases your descriptor. Unlike `close`, a socket shutdown also affects other descriptors referring to that same socket.

## Quick return-value reference

| Functions                                                | What to check                                                        |
| -------------------------------------------------------- | -------------------------------------------------------------------- |
| `socket`, `accept`                                       | A descriptor `>= 0`; `-1` means failure.                             |
| `bind`, `listen`, `connect`, `pipe`, `shutdown`, `close` | `0` means success; `-1` means failure.                               |
| `inet_pton`                                              | `1` means converted; `0` means invalid text; `-1` means error.       |
| `read`                                                   | Positive count means data; `0` means EOF; `-1` means failure.        |
| `write`                                                  | Number of bytes written; `-1` means failure.                         |
| `poll`                                                   | Positive ready-entry count; `0` means timeout; `-1` means failure.   |
| `fork`                                                   | `0` in the child; positive child PID in the parent; `-1` on failure. |
| `waitpid` with options `0`                               | The collected child's PID; `-1` means failure.                       |
| `dup2`                                                   | The destination descriptor; `-1` means failure.                      |
| `execl`, `execlp`                                        | Successful calls never return; `-1` means failure.                   |

For more detail offline, use the local manual pages, for example `man 2 socket`, `man 2 poll`, `man 2 fork`, `man 2 dup2`, `man 2 pipe`, and `man 3 exec`.
