# reactor/Makefile

CXX = g++
CXXFLAGS = -Wall -std=c++17 -pthread -O2
AR = ar
ARFLAGS = rcs

SRC = Reactor_test.cpp
OBJ = $(SRC:.cpp=.o)
LIB = libreac.a

all: $(LIB)

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o $(LIB)
