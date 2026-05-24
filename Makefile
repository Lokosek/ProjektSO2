CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread
TARGETS = most_warunkowe most_semafory

.PHONY: all clean

all: $(TARGETS)

most_warunkowe: src/most_warunkowe.cpp
	$(CXX) $(CXXFLAGS) src/most_warunkowe.cpp -o most_warunkowe

most_semafory: src/most_semafory.cpp
	$(CXX) $(CXXFLAGS) src/most_semafory.cpp -o most_semafory

clean:
	rm -f $(TARGETS) bridge bridge_condition bridge_semaphore
