CC = g++

.PHONY: all clean

# MARK: Binaries

all: zoo zoo_info zoo_client libzoomap.so

zoo: main.o libzoomap.so
	$(CC) $< -L. -lzoomap -o $@

zoo_info: zoo_info.o
	$(CC) $< -o $@

zoo_client: zoo_client.o libzoomap.so
	$(CC) $< -L. -lzoomap -o $@

# MARK: Object files

main.o: main.cpp io.hpp zoo_map.hpp
	$(CC) -c $< -o $@

zoo_info.o: zoo_info.cpp
	$(CC) -c $< -o $@

zoo_client.o: zoo_client.cpp io.hpp
	$(CC) -c $< -o $@

# MARK: Clean

clean:
	rm -f *.o zoo zoo_info zoo_client
