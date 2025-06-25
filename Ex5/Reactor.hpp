#ifndef REACTOR_HPP
#define REACTOR_HPP

// Function pointer type for reactor callbacks
typedef void* (*reactorFunc)(int fd);

// Reactor structure declaration
struct Reactor;

// Starts new reactor and returns pointer to it
void* startReactor();

// Adds fd to Reactor (for reading); returns 0 on success
int addFdToReactor(void* reactor, int fd, reactorFunc func);

// Removes fd from reactor
int removeFdFromReactor(void* reactor, int fd);

// Stops reactor
int stopReactor(void* reactor);


#endif // REACTOR_HPP
