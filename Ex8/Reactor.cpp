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
#include <pthread.h>
#include <unistd.h>
#include <set>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdio>

// Function pointer type for reactor callbacks
typedef void* (*reactorFunc)(int fd);

// פונקציית callback פר לקוח
typedef void* (*proactorFunc)(int sockfd);

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

struct Proactor {
    int listener_fd;
    proactorFunc handlerFunc;
    bool running;
    pthread_t thread;
    std::mutex graph_mutex;  // להגנה על משאבים משותפים
    std::set<pthread_t> client_threads;
};

// Thread per connection
void* handle_connection_thread(void* arg) {
    auto* data = static_cast<std::pair<proactorFunc, int>*>(arg);
    proactorFunc func = data->first;
    int client_fd = data->second;
    delete data;

    if (func) func(client_fd);
    close(client_fd);
    return nullptr;
}

void* proactor_loop(void* arg) {
    Proactor* p = static_cast<Proactor*>(arg);

    while (p->running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(p->listener_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        pthread_t tid;
        auto* args = new std::pair<proactorFunc, int>(p->handlerFunc, client_fd);
        if (pthread_create(&tid, nullptr, handle_connection_thread, args) == 0) {
            std::lock_guard<std::mutex> lock(p->graph_mutex);
            p->client_threads.insert(tid);
        } else {
            perror("pthread_create");
            delete args;
            close(client_fd);
        }
    }

    return nullptr;
}

// Start the proactor on given socket and callback
pthread_t startProactor(int sockfd, proactorFunc threadFunc) {
    Proactor* p = new Proactor();
    p->listener_fd = sockfd;
    p->handlerFunc = threadFunc;
    p->running = true;

    if (pthread_create(&p->thread, nullptr, proactor_loop, p) != 0) {
        perror("pthread_create");
        delete p;
        return 0;
    }

    return p->thread;
}

// Stop the proactor by thread id
int stopProactor(pthread_t tid) {
    // We assume the Proactor object is accessible by thread id
    // For real system, consider keeping mapping tid -> Proactor*
    // This implementation just cancels the thread for simplicity

    if (pthread_cancel(tid) != 0) {
        perror("pthread_cancel");
        return -1;
    }

    if (pthread_join(tid, nullptr) != 0) {
        perror("pthread_join");
        return -1;
    }

    return 0;
}


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