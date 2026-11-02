CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -pthread

.PHONY: all clean

# MARK: Binaries

a.out: main.o
	$(CXX) $(CXXFLAGS) $< -o $@

# MARK: Object files

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# MARK: Clean

clean:
	rm -f *.o a.out
