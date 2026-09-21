CC = g++

.PHONY: all clean

all: transform transform-dynamic dynamic_upper/libtransform.so dynamic_lower/libtransform.so

# MARK: Binaries

transform: main.o libtransform.a
	$(CC) $< -L. -ltransform -o $@

transform-dynamic: main.o dynamic_upper/libtransform.so
	$(CC) $< -Ldynamic_upper -ltransform -o $@

# MARK: Shared Objects

main.o: main.cpp transform.hpp
	$(CC) -c $< -o $@

transform.o: transform_upper.cpp transform.hpp
	$(CC) -c $< -o $@

dynamic_upper/transform.pic.o: transform_upper.cpp transform.hpp
	mkdir -p dynamic_upper
	$(CC) -fPIC -c $< -o $@

dynamic_lower/transform.pic.o: transform_lower.cpp transform.hpp
	mkdir -p dynamic_lower
	$(CC) -fPIC -c $< -o $@

# MARK: Libraries

libtransform.a: transform.o
	ar rcs $@ $^

dynamic_upper/libtransform.so: dynamic_upper/transform.pic.o
	mkdir -p dynamic_upper
	$(CC) -shared $^ -o $@

dynamic_lower/libtransform.so: dynamic_lower/transform.pic.o
	mkdir -p dynamic_lower
	$(CC) -shared $^ -o $@

# MARK: Clean

clean:
	rm -f transform transform-dynamic *.o *.a *.so
	rm -f dynamic_upper/*.o dynamic_upper/*.so
	rm -f dynamic_lower/*.o dynamic_lower/*.so
