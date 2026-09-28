CC = g++

.PHONY: all clean

all: log_check liblog.so

# MARK: Binaries

log_check: main.o liblog.so
	$(CC) $< -L. -llog -o $@

# MARK: Object files

main.o: main.cpp log.hpp
	$(CC) -c $< -o $@

log.pic.o: log.cpp log.hpp
	$(CC) -fPIC -c $< -o $@

# MARK: Libraries

liblog.so: log.pic.o
	$(CC) -shared $^ -o $@

# MARK: Clean

clean:
	rm -f log_check *.o *.a *.so
