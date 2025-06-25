CXX = g++
CXXFLAGS = -Wall -g -pthread -std=c++11 -I../Ex5

all: server client

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp -L../Ex5 -lreac

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp

clean:
	rm -f server client