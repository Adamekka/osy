CC = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -pthread

.PHONY: all clean

ifeq ($(shell uname -s),Darwin)
# The supplied Linux library cannot be linked by the macOS linker.
all:
	docker build --platform linux/amd64 -t osy-2-2 .
else

# MARK: Binaries

all: zoo zoo_info zoo_client libzoomap.so

zoo: main.o
	$(CC) $(CXXFLAGS) $< -o $@

zoo_info: zoo_info.o
	$(CC) $(CXXFLAGS) $< -o $@

zoo_client: zoo_client.o libzoomap.so
	$(CC) $(CXXFLAGS) $< -L. -lzoomap -Wl,-rpath,. -o $@

# MARK: Object files

main.o: main.cpp io.hpp zoo_map.hpp
	$(CC) $(CXXFLAGS) -c $< -o $@

zoo_info.o: zoo_info.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

zoo_client.o: zoo_client.cpp io.hpp zoo_map.hpp
	$(CC) $(CXXFLAGS) -c $< -o $@

endif

# MARK: Clean

clean:
	rm -f *.o zoo zoo_info zoo_client
