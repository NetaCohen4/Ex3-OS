CXX = g++
CXXFLAGS = -Wall -pthread -std=c++11 -I../Ex5

all: server_test client

server_test: server_test.cpp
	$(CXX) $(CXXFLAGS) -o server_test server_test.cpp -L../Ex5 -lreac

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp

clean:
	rm -f server client