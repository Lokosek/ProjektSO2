CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread
TARGET = bridge

all:
	$(CXX) $(CXXFLAGS) src/main.cpp -o $(TARGET)

clean:
	rm -f $(TARGET)
