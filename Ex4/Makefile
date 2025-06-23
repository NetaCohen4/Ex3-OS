CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

TARGETS = client server

all: $(TARGETS)

# CH: CH.cpp
# 	$(CXX) $(CXXFLAGS) -o CH CH.cpp

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp

client1: client.cpp
	$(CXX) $(CXXFLAGS) -o client1 client1.cpp

clean:
	rm -f $(TARGETS)
