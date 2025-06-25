#ifndef REACTOR_HPP
#define REACTOR_HPP

#include <vector>
#include <map>
#include <sys/select.h>
#include <poll.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <pthread.h>
#include <stdio.h>
#include "Reactor.hpp"

// Function pointer type for reactor callbacks
typedef void* (*reactorFunc)(int fd);

// Reactor structure
struct Reactor {
    std::vector<int> fds;                    // List of file descriptors
    std::map<int, reactorFunc> reactorFuncs; // Map fd -> function
    bool running;                            // Reactor state
    pthread_t thread;                        // Thread running the reactor loop
    pthread_mutex_t mutex;                   // Protect fds and reactorFuncs

    Reactor() : running(false) {
        pthread_mutex_init(&mutex, nullptr);
    }
    ~Reactor() {
        pthread_mutex_destroy(&mutex);
    }
};

// Internal reactor loop function (select)
void* reactorFunction(void* reactor_ptr) {
    Reactor* reactor = static_cast<Reactor*>(reactor_ptr);
    while (reactor->running) {
        if (reactor->fds.empty()) {
            usleep(1000);
            continue;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = 0;

        pthread_mutex_lock(&reactor->mutex);
        for (int fd : reactor->fds) {
            FD_SET(fd, &readfds);
            if (fd > max_fd) max_fd = fd;
        }
        pthread_mutex_unlock(&reactor->mutex);

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int result = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (result == -1) {
            perror("select");
            break;
        } else if (result == 0) {
            continue;
        }
        
        // Copy fds to avoid holding the mutex while processing
        pthread_mutex_lock(&reactor->mutex);
        std::vector<int> fds_copy = reactor->fds;
        pthread_mutex_unlock(&reactor->mutex);

        for (int fd : fds_copy) {
            if (FD_ISSET(fd, &readfds)) {
                pthread_mutex_lock(&reactor->mutex);
                auto it = reactor->reactorFuncs.find(fd);
                reactorFunc func = (it != reactor->reactorFuncs.end()) ? it->second : nullptr;
                pthread_mutex_unlock(&reactor->mutex);
                if (func) func(fd);
            }
        }
    }
    return nullptr;
}

// Starts new reactor and returns pointer to it
void* startReactor() {
    Reactor* reactor = new Reactor();
    reactor->running = true;
    if (pthread_create(&reactor->thread, nullptr, reactorFunction, reactor) != 0) {
        delete reactor;
        return nullptr;
    }
    return static_cast<void*>(reactor);
}

// Adds fd to Reactor (for reading); returns 0 on success
int addFdToReactor(void* reactor, int fd, reactorFunc func) {
    if (reactor == nullptr || func == nullptr) return -1;
    Reactor* r = static_cast<Reactor*>(reactor);
    pthread_mutex_lock(&r->mutex);
    auto it = std::find(r->fds.begin(), r->fds.end(), fd);
    if (it != r->fds.end()) {
        pthread_mutex_unlock(&r->mutex);
        return -1;
    }
    r->fds.push_back(fd);
    r->reactorFuncs[fd] = func;
    pthread_mutex_unlock(&r->mutex);
    return 0;
}

// Removes fd from reactor
int removeFdFromReactor(void* reactor, int fd) {
    if (reactor == nullptr) return -1;
    Reactor* r = static_cast<Reactor*>(reactor);
    pthread_mutex_lock(&r->mutex);
    auto it = std::find(r->fds.begin(), r->fds.end(), fd);
    if (it == r->fds.end()) {
        pthread_mutex_unlock(&r->mutex);
        return -1;
    }
    r->fds.erase(it);
    r->reactorFuncs.erase(fd);
    pthread_mutex_unlock(&r->mutex);
    return 0;
}

// Stops reactor
int stopReactor(void* reactor) {
    if (reactor == nullptr) return -1;
    Reactor* r = static_cast<Reactor*>(reactor);
    r->running = false;
    pthread_join(r->thread, nullptr);
    delete r;
    return 0;
}


#endif // REACTOR_HPP