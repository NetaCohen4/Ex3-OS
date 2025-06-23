#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <thread>

using namespace std;

#define PORT "9034"  // Port to connect to
#define MAXDATASIZE 1024 // Max number of bytes we can get at once

// Get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Connect to server
int connect_to_server(const char* hostname) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // Loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        freeaddrinfo(servinfo);
        return -1;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // All done with this structure
    return sockfd;
}

// Thread function to receive messages from server
void receive_messages(int sockfd) {
    char buf[MAXDATASIZE];
    int numbytes;
    
    while (true) {
        if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
            perror("recv");
            break;
        }
        
        if (numbytes == 0) {
            printf("Server closed the connection.\n");
            break;
        }
        
        buf[numbytes] = '\0';
        printf("Server response: %s", buf);
        fflush(stdout);
    }
}

// Interactive mode - user can type commands
void interactive_mode(int sockfd) {
    string line;
    
    cout << "Available commands:" << endl;
    cout << "  Newgraph n        - Create new graph with n points" << endl;
    cout << "  x,y               - Add point (x,y) to current graph" << endl;
    cout << "  Newpoint x,y      - Add new point (x,y)" << endl;
    cout << "  Removepoint x,y   - Remove point (x,y)" << endl;
    cout << "  CH                - Calculate convex hull area" << endl;
    cout << "  quit              - Exit client" << endl << endl;
    
    while (true) {
        if (!getline(cin, line)) {
            break;
        }
        
        if (line == "quit" || line == "exit") {
            break;
        }
        
        if (line.empty()) {
            continue;
        }
        
        // Send command to server
        line += "\n";
        if (send(sockfd, line.c_str(), line.length(), 0) == -1) {
            perror("send");
            break;
        }
    }
}


int main() {
    int sockfd;
    const char* hostname = "localhost";
    
    // Connect to server
    sockfd = connect_to_server(hostname);
    if (sockfd == -1) {
        exit(1);
    }
    
    printf("Connected to CH Server on port %s\n", PORT);
    
    // Start receiving thread
    thread receiver_thread(receive_messages, sockfd);
    receiver_thread.detach();
    
    // Give receiver thread time to start
    this_thread::sleep_for(chrono::milliseconds(100));
    
    interactive_mode(sockfd);
    
    
    close(sockfd);
    return 0;
}