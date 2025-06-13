CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17
TARGET = CH
SRC = CH.cpp


all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
