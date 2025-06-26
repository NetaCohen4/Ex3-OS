#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <deque>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <netdb.h>

using namespace std;

#define PORT "9034"  // Port we're listening on

struct Point {
    float x, y;

    bool operator<(const Point &other) const {
        return x < other.x || (x == other.x && y < other.y);
    }
};

// Global graph data structure shared by all clients
vector<Point> global_points;

// Cross product
float cross(const Point &O, const Point &A, const Point &B) {
    return (A.x - O.x)*(B.y - O.y) - (A.y - O.y)*(B.x - O.x);
}

// Convex Hull algorithm using deque
vector<Point> convexHullDeque(vector<Point> P) {
    int n = P.size();
    if (n < 3) return P;
    
    sort(P.begin(), P.end());
    deque<Point> hull;

    for (int i = 0; i < n; i++) {
        while (hull.size() >= 2 && cross(hull[hull.size()-2], hull.back(), P[i]) <= 0)
            hull.pop_back();
        hull.push_back(P[i]);
    }

    long unsigned int t = hull.size() + 1;
    for (int i = n - 2; i >= 0; i--) {
        while (hull.size() >= t && cross(hull[hull.size()-2], hull.back(), P[i]) <= 0)
            hull.pop_back();
        hull.push_back(P[i]);
    }

    hull.pop_back(); // remove duplicate
    return vector<Point>(hull.begin(), hull.end());
}

// Calculate polygon area
float polygonArea(const vector<Point> &poly) {
    if (poly.size() < 3) return 0.0;
    
    float area = 0;
    int n = poly.size();
    for (int i = 0; i < n; ++i) {
        area += poly[i].x * poly[(i+1)%n].y - poly[i].y * poly[(i+1)%n].x;
    }
    return abs(area) / 2.0;
}

// Get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Return a listening socket
int get_listener_socket(void) {
    int listener;     // Listening socket descriptor
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai); // All done with this
    
    if (p == NULL) {
        return -1;
    }

    // Listen
    if (listen(listener, 10) == -1) {
        return -1;
    }

    return listener;
}

// Add a new file descriptor to the set
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size) {
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2; // Double it
        *pfds = (struct pollfd*)realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

// Remove an index from the set
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count) {
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];
    (*fd_count)--;
}

// Process client command
string process_command(const string& command) {
    stringstream ss(command);
    string cmd;
    ss >> cmd;
    
    if (cmd == "Newgraph") {
        int n;
        ss >> n;
        global_points.clear();
        return "OK: New graph created with " + to_string(n) + " points expected\n";
        
    } else if (cmd == "CH") {
        auto hull = convexHullDeque(global_points);
        float area = polygonArea(hull);
        return to_string(area) + "\n";
        
    } else if (cmd == "Newpoint") {
        float x, y;
        char comma;
        ss >> x >> comma >> y;
        global_points.push_back({x, y});
        return "OK: Point added\n";
        
    } else if (cmd == "Removepoint") {
        float x, y;
        char comma;
        ss >> x >> comma >> y;
        auto it = remove_if(global_points.begin(), global_points.end(), 
                           [x, y](const Point& p) {
                               return abs(p.x - x) < 0.0001 && abs(p.y - y) < 0.0001;
                           });
        if (it != global_points.end()) {
            global_points.erase(it, global_points.end());
            return "OK: Point removed\n";
        } else {
            return "ERROR: Point not found\n";
        }
    } else {
        // Handle point coordinates for Newgraph
        float x, y;
        if (ss.str().find(',') != string::npos) {
            string line = ss.str();
            replace(line.begin(), line.end(), ',', ' ');
            stringstream pointStream(line);
            if (pointStream >> x >> y) {
                global_points.push_back({x, y});
                return "OK: Point added to graph\n";
            }
        }
        return "ERROR: Unknown command\n";
    }
}

// Handle incoming connections
void handle_new_connection(int listener, int *fd_count, int *fd_size, struct pollfd **pfds) {
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;
    int newfd; // Newly accept()ed socket descriptor
    char remoteIP[INET6_ADDRSTRLEN];

    addrlen = sizeof remoteaddr;
    newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);
    
    if (newfd == -1) {
        perror("accept");
    } else {
        add_to_pfds(pfds, newfd, fd_count, fd_size);
        printf("CH Server: new connection from %s on socket %d\n",
               inet_ntop(remoteaddr.ss_family,
                        get_in_addr((struct sockaddr*)&remoteaddr),
                        remoteIP, INET6_ADDRSTRLEN),
               newfd);
    }
}

// Handle regular client data or client hangups
void handle_client_data(int *fd_count, struct pollfd *pfds, int *pfd_i) {
    char buf[1024]; // Buffer for client data
    int nbytes = recv(pfds[*pfd_i].fd, buf, sizeof buf - 1, 0);
    int sender_fd = pfds[*pfd_i].fd;
    
    if (nbytes <= 0) {
        // Got error or connection closed by client
        if (nbytes == 0) {
            // Connection closed
            printf("CH Server: socket %d hung up\n", sender_fd);
        } else {
            perror("recv");
        }
        close(pfds[*pfd_i].fd); // Bye!
        del_from_pfds(pfds, *pfd_i, fd_count);
        (*pfd_i)--; // Reexamine the slot we just deleted
        
    } else {
        // We got some good data from a client
        buf[nbytes] = '\0'; // Null terminate the string
        
        printf("CH Server: received from fd %d: %s", sender_fd, buf);
        
        // Process the command
        string command(buf);
        // Remove trailing newline if present
        if (!command.empty() && command.back() == '\n') {
            command.pop_back();
        }
        
        string response = process_command(command);
        
        // Send response back to the client
        if (send(sender_fd, response.c_str(), response.length(), 0) == -1) {
            perror("send");
        }
    }
}

// Process all existing connections
void process_connections(int listener, int *fd_count, int *fd_size, struct pollfd **pfds) {
    for(int i = 0; i < *fd_count; i++) {
        // Check if someone's ready to read
        if ((*pfds)[i].revents & (POLLIN | POLLHUP)) {
            // We got one!!
            if ((*pfds)[i].fd == listener) {
                // If we're the listener, it's a new connection
                handle_new_connection(listener, fd_count, fd_size, pfds);
            } else {
                // Otherwise we're just a regular client
                handle_client_data(fd_count, *pfds, &i);
            }
        }
    }
}

// Main: create a listener and connection set, loop forever processing connections
int main(void) {
    int listener; // Listening socket descriptor
    
    // Start off with room for 5 connections (We'll realloc as necessary)
    int fd_size = 5;
    int fd_count = 0;
    struct pollfd *pfds = (struct pollfd*)malloc(sizeof *pfds * fd_size);
    
    // Set up and get a listening socket
    listener = get_listener_socket();
    if (listener == -1) {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }
    
    // Add the listener to set; Report ready to read on incoming connection
    pfds[0].fd = listener;
    pfds[0].events = POLLIN;
    fd_count = 1; // For the listener
    
    printf("CH Server: waiting for connections on port %s...\n", PORT);
    
    // Main loop
    for(;;) {
        int poll_count = poll(pfds, fd_count, -1);
        
        if (poll_count == -1) {
            perror("poll");
            exit(1);
        }
        
        // Run through connections looking for data to read
        process_connections(listener, &fd_count, &fd_size, &pfds);
    }
    
    free(pfds);
    return 0;
}