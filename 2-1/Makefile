CC = g++

.PHONY: all clean

# MARK: Binaries

all: zoo zoo_info zoo_client

zoo: main.o
	$(CC) $< -o $@

zoo_info: zoo_info.o
	$(CC) $< -o $@

zoo_client: zoo_client.o
	$(CC) $< -o $@

# MARK: Object files

main.o: main.cpp io.hpp
	$(CC) -c $< -o $@

zoo_info.o: zoo_info.cpp
	$(CC) -c $< -o $@

zoo_client.o: zoo_client.cpp io.hpp
	$(CC) -c $< -o $@

# MARK: Clean

clean:
	rm -f *.o zoo zoo_info zoo_client
