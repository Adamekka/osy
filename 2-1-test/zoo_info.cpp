#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>

// Called as zoo_info <animal>. stdout may be a terminal or the client socket,
// depending on whether the program was started manually or through the server.
auto main(const int32_t argc, char** const argv) -> int {
    assert(argc == 2);

    const auto animal{std::string{argv[1]}};
    if (animal == "mouse") {
        // Use the mouse values supplied in the assignment's example.
        std::puts(
            "Zvíře: myš\n"
            "Výška: 5 cm\n"
            "Hmotnost: 150 g\n"
            "Potrava: všežravec"
        );
        return 0;
    }

    // Unknown animals are a query failure. The server forwards this message and
    // waits for us to exit, then continues handling the visitor's commands.
    std::puts("Neznámé zvíře.");
    return 1;
}
