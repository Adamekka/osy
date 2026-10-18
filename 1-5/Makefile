CC = g++

.PHONY: all clean

# MARK: Binaries

chat: main.o
	$(CC) $< -o $@

# MARK: Object files

main.o: main.cpp
	$(CC) -c $< -o $@

# MARK: Clean

clean:
	rm -f *.o chat
