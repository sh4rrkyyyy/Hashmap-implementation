
CXX = g++
CXXFLAGS = -std=c++20 -O2 -Wall  -Iinclude

TEST = tests/test.cpp
CATCH = catch.hpp
TARGET = test

all: $(TARGET)

$(TARGET): $(SRC) $(TEST) $(CATCH)
	$(CXX) $(CXXFLAGS) $(SRC) $(TEST) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)