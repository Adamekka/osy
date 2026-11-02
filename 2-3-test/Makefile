CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -pthread

.PHONY: all clean

all: pizzeria pizzeria_client

pizzeria: main.o
	$(CXX) $(CXXFLAGS) $< -o $@

pizzeria_client: client.o
	$(CXX) $(CXXFLAGS) $< -o $@

main.o: main.cpp io.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

client.o: client.cpp io.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o pizzeria pizzeria_client
