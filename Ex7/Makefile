CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

TARGETS = client server_test

all: $(TARGETS)

# CH: CH.cpp
# 	$(CXX) $(CXXFLAGS) -o CH CH.cpp

server: server_test.cpp
	$(CXX) $(CXXFLAGS) -o server_test server_test.cpp

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp

clean:
	rm -f $(TARGETS)
