CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

TARGETS = CH client

all: $(TARGETS)

CH: CH.cpp
	$(CXX) $(CXXFLAGS) -o CH CH.cpp

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp

clean:
	rm -f $(TARGETS)
