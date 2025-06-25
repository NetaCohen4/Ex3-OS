#ifndef REACTOR_HPP
#define REACTOR_HPP

// Function pointer type for reactor callbacks
typedef void *(*reactorFunc)(int fd);

// Proactor function pointer type
typedef void *(*proactorFunc)(int sock_fd);

// starts new proactor and returns proactor thread id.
pthread_t startProactor(int sockfd, proactorFunc threadFunc);

// stops proactor by threadid
int stopProactor(pthread_t tid);

// Reactor structure declaration
struct Reactor;

// Starts new reactor and returns pointer to it
void *startReactor();

// Adds fd to Reactor (for reading); returns 0 on success
int addFdToReactor(void *reactor, int fd, reactorFunc func);

// Removes fd from reactor
int removeFdFromReactor(void *reactor, int fd);

// Stops reactor
int stopReactor(void *reactor);

#endif // REACTOR_HPP
