#ifndef REACTOR_HPP
#define REACTOR_HPP

#include <vector>
#include <map>
#include <sys/select.h>
#include <poll.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <stdio.h>
#include <pthread.h>



// Function pointer type for reactor callbacks
typedef void* (*reactorFunc)(int fd);

// Reactor structure
struct Reactor {
    std::vector<int> fds;                    // List of file descriptors
    std::map<int, reactorFunc> reactorFuncs; // Map fd -> function
    bool running;                            // Reactor state
    int max_fd;                             // Maximum fd for select
    
    Reactor() : running(false), max_fd(0) {}
};

// Forward declarations
void* startReactor();
int addFdToReactor(void* reactor, int fd, reactorFunc func);
int removeFdFromReactor(void* reactor, int fd);
int stopReactor(void* reactor);

// Internal reactor loop function
void* reactorFunction(void* reactor_ptr) {
    Reactor* reactor = static_cast<Reactor*>(reactor_ptr);
    
    while (reactor->running) {
        if (reactor->fds.empty()) {
            // No file descriptors to monitor, sleep briefly
            usleep(1000); // 1ms
            continue;
        }
        
        fd_set readfds;
        FD_ZERO(&readfds);
        
        // Add all fds to the select set
        int max_fd = 0;
        for (int fd : reactor->fds) {
            FD_SET(fd, &readfds);
            if (fd > max_fd) {
                max_fd = fd;
            }
        }
        reactor->max_fd = max_fd;
        
        // Set timeout for select (1 second)
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int result = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        
        if (result == -1) {
            perror("select");
            break;
        } else if (result == 0) {
            // Timeout, continue loop
            continue;
        }
        
        // Check which fds are ready and call their functions
        for (int fd : reactor->fds) {
            if (FD_ISSET(fd, &readfds)) {
                auto it = reactor->reactorFuncs.find(fd);
                if (it != reactor->reactorFuncs.end() && it->second != nullptr) {
                    it->second(fd);
                }
            }
        }
    }
    
    return nullptr;
}

// Alternative reactor implementation using poll()
void* reactorFunctionPoll(void* reactor_ptr) {
    Reactor* reactor = static_cast<Reactor*>(reactor_ptr);
    
    while (reactor->running) {
        if (reactor->fds.empty()) {
            usleep(1000); // 1ms
            continue;
        }
        
        // Prepare pollfd array
        std::vector<struct pollfd> pollfds;
        for (int fd : reactor->fds) {
            struct pollfd pfd;
            pfd.fd = fd;
            pfd.events = POLLIN;
            pfd.revents = 0;
            pollfds.push_back(pfd);
        }
        
        // Poll with 1 second timeout
        int result = poll(pollfds.data(), pollfds.size(), 1000);
        
        if (result == -1) {
            perror("poll");
            break;
        } else if (result == 0) {
            // Timeout, continue loop
            continue;
        }
        
        // Check which fds are ready and call their functions
        for (const auto& pfd : pollfds) {
            if (pfd.revents & POLLIN) {
                auto it = reactor->reactorFuncs.find(pfd.fd);
                if (it != reactor->reactorFuncs.end() && it->second != nullptr) {
                    it->second(pfd.fd);
                }
            }
        }
    }
    
    return nullptr;
}

// Starts new reactor and returns pointer to it
void* startReactor() {
    Reactor* reactor = new Reactor();
    if (!reactor) return nullptr;

    reactor->running = true;

    return static_cast<void*>(reactor);
}

// Adds fd to Reactor (for reading); returns 0 on success
int addFdToReactor(void* reactor, int fd, reactorFunc func) {
    if (reactor == nullptr || func == nullptr) {
        return -1;
    }
    
    Reactor* r = static_cast<Reactor*>(reactor);
    
    // Check if fd already exists
    auto it = std::find(r->fds.begin(), r->fds.end(), fd);
    if (it != r->fds.end()) {
        return -1; // fd already exists
    }
    
    r->fds.push_back(fd);
    r->reactorFuncs[fd] = func;
    
    return 0;
}

// Removes fd from reactor
int removeFdFromReactor(void* reactor, int fd) {
    if (reactor == nullptr) {
        return -1;
    }
    
    Reactor* r = static_cast<Reactor*>(reactor);
    
    // Remove from fds vector
    auto it = std::find(r->fds.begin(), r->fds.end(), fd);
    if (it == r->fds.end()) {
        return -1; // fd not found
    }
    
    r->fds.erase(it);
    r->reactorFuncs.erase(fd);
    
    return 0;
}

// Stops reactor
int stopReactor(void* reactor) {
    if (reactor == nullptr) {
        return -1;
    }
    
    Reactor* r = static_cast<Reactor*>(reactor);
    r->running = false;
    
    // Clean up
    r->fds.clear();
    r->reactorFuncs.clear();
    
    delete r;
    return 0;
}

// Run the reactor loop (blocking call)
void runReactor(void* reactor) {
    if (reactor == nullptr) {
        return;
    }
    
    // Choose between select and poll implementation
    #ifdef USE_POLL
        reactorFunctionPoll(reactor);
    #else
        reactorFunction(reactor);
    #endif
}

#endif // REACTOR_HPP