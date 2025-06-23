#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <map>
#include "../Ex5/Reactor.hpp" // או "reactor.hpp" אם הוא בתיקיה הנוכחית

void* reactor_ptr = nullptr; // גלובלי עבור removeFdFromReactor


struct Point {
    double x, y;
    Point(double x = 0, double y = 0) : x(x), y(y) {}
    bool operator<(const Point& other) const {
        if (x != other.x) return x < other.x;
        return y < other.y;
    }
    bool operator==(const Point& other) const {
        return std::abs(x - other.x) < 1e-9 && std::abs(y - other.y) < 1e-9;
    }
};

std::vector<Point> graph;

// Cross product for orientation
double cross(const Point& o, const Point& a, const Point& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// Convex Hull using Graham Scan
std::vector<Point> convexHull(std::vector<Point> points) {
    if (points.size() <= 1) return points;
    int min_idx = 0;
    for (int i = 1; i < (int)points.size(); i++) {
        if (points[i].y < points[min_idx].y || 
            (points[i].y == points[min_idx].y && points[i].x < points[min_idx].x)) {
            min_idx = i;
        }
    }
    std::swap(points[0], points[min_idx]);
    Point pivot = points[0];
    std::sort(points.begin() + 1, points.end(), [&](const Point& a, const Point& b) {
        double cross_prod = cross(pivot, a, b);
        if (cross_prod == 0) {
            double dist_a = (a.x - pivot.x) * (a.x - pivot.x) + (a.y - pivot.y) * (a.y - pivot.y);
            double dist_b = (b.x - pivot.x) * (b.x - pivot.x) + (b.y - pivot.y) * (b.y - pivot.y);
            return dist_a < dist_b;
        }
        return cross_prod > 0;
    });
    std::vector<Point> hull;
    for (const Point& p : points) {
        while (hull.size() >= 2 && cross(hull[hull.size()-2], hull[hull.size()-1], p) < 0) {
            hull.pop_back();
        }
        hull.push_back(p);
    }
    return hull;
}

// Calculate area of convex hull
double calculateArea(const std::vector<Point>& hull) {
    if (hull.size() < 3) return 0.0;
    double area = 0.0;
    int n = hull.size();
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        area += hull[i].x * hull[j].y;
        area -= hull[j].x * hull[i].y;
    }
    return std::abs(area) / 2.0;
}

// Client data structure
struct ClientData {
    std::string buffer;
    ClientData() {}
};
std::map<int, ClientData> clients;

struct ClientState {
    bool waiting_for_points = false;
    int points_remaining = 0;
};
std::map<int, ClientState> client_states;

void processCommand(int client_fd, const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;

    if (client_states[client_fd].waiting_for_points) {
        size_t comma_pos = command.find(',');
        if (comma_pos != std::string::npos) {
            try {
                double x = std::stod(command.substr(0, comma_pos));
                double y = std::stod(command.substr(comma_pos + 1));
                graph.push_back(Point(x, y));
                client_states[client_fd].points_remaining--;
                if (client_states[client_fd].points_remaining == 0) {
                    client_states[client_fd].waiting_for_points = false;
                    std::string response = "Graph created with " + std::to_string(graph.size()) + " points\n";
                    send(client_fd, response.c_str(), response.length(), 0);
                }
            } catch (...) {
                std::string error = "Invalid point format\n";
                send(client_fd, error.c_str(), error.length(), 0);
                client_states[client_fd].waiting_for_points = false;
            }
        }
        return;
    }

    if (cmd == "Newgraph") {
        int n;
        if (iss >> n) {
            graph.clear();
            if (n > 0) {
                client_states[client_fd].waiting_for_points = true;
                client_states[client_fd].points_remaining = n;
                std::string response = "Ready to receive " + std::to_string(n) + " points. Send them as x,y format:\n";
                send(client_fd, response.c_str(), response.length(), 0);
            } else {
                std::string response = "Empty graph created\n";
                send(client_fd, response.c_str(), response.length(), 0);
            }
        } else {
            std::string error = "Invalid Newgraph command format\n";
            send(client_fd, error.c_str(), error.length(), 0);
        }
    } else if (cmd == "CH") {
        std::vector<Point> hull = convexHull(graph);
        double area = calculateArea(hull);
        std::string response = std::to_string(area) + "\n";
        send(client_fd, response.c_str(), response.length(), 0);
    } else if (cmd == "Newpoint") {
        std::string coords;
        iss >> coords;
        size_t comma_pos = coords.find(',');
        if (comma_pos != std::string::npos) {
            try {
                double x = std::stod(coords.substr(0, comma_pos));
                double y = std::stod(coords.substr(comma_pos + 1));
                graph.push_back(Point(x, y));
                std::string response = "Point added\n";
                send(client_fd, response.c_str(), response.length(), 0);
            } catch (...) {
                std::string error = "Invalid point format\n";
                send(client_fd, error.c_str(), error.length(), 0);
            }
        } else {
            std::string error = "Invalid point format\n";
            send(client_fd, error.c_str(), error.length(), 0);
        }
    } else if (cmd == "Removepoint") {
        std::string coords;
        iss >> coords;
        size_t comma_pos = coords.find(',');
        if (comma_pos != std::string::npos) {
            try {
                double x = std::stod(coords.substr(0, comma_pos));
                double y = std::stod(coords.substr(comma_pos + 1));
                auto it = std::find(graph.begin(), graph.end(), Point(x, y));
                if (it != graph.end()) {
                    graph.erase(it);
                    std::string response = "Point removed\n";
                    send(client_fd, response.c_str(), response.length(), 0);
                } else {
                    std::string response = "Point not found\n";
                    send(client_fd, response.c_str(), response.length(), 0);
                }
            } catch (...) {
                std::string error = "Invalid point format\n";
                send(client_fd, error.c_str(), error.length(), 0);
            }
        } else {
            std::string error = "Invalid point format\n";
            send(client_fd, error.c_str(), error.length(), 0);
        }
    } else {
        size_t comma_pos = command.find(',');
        if (comma_pos != std::string::npos) {
            try {
                double x = std::stod(command.substr(0, comma_pos));
                double y = std::stod(command.substr(comma_pos + 1));
                graph.push_back(Point(x, y));
                std::string response = "Point added\n";
                send(client_fd, response.c_str(), response.length(), 0);
            } catch (...) {
                std::string error = "Unknown command or invalid format\n";
                send(client_fd, error.c_str(), error.length(), 0);
            }
        } else {
            std::string error = "Unknown command\n";
            send(client_fd, error.c_str(), error.length(), 0);
        }
    }
}

void* clientCallback(int client_fd) {
    char buffer[1024];
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        if (bytes == 0) {
            std::cout << "Client " << client_fd << " disconnected normally\n";
        } else {
            perror("recv");
        }
        clients.erase(client_fd);
        client_states.erase(client_fd);
        removeFdFromReactor(reactor_ptr, client_fd);
        close(client_fd);
        return nullptr;
    }
    buffer[bytes] = '\0';
    clients[client_fd].buffer += buffer;
    size_t pos;
    while ((pos = clients[client_fd].buffer.find('\n')) != std::string::npos) {
        std::string command = clients[client_fd].buffer.substr(0, pos);
        clients[client_fd].buffer.erase(0, pos + 1);
        if (!command.empty() && command.back() == '\r') command.pop_back();
        processCommand(client_fd, command);
    }
    return nullptr;
}

void* acceptCallback(int listen_fd) {
    std::cout << "Accepting new client connection...\n";
    sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &addrlen);
    if (client_fd < 0) {
        perror("accept");
        return nullptr;
    }
    clients[client_fd] = ClientData();
    addFdToReactor(reactor_ptr, client_fd, clientCallback);
    std::cout << "New client connected: " << client_fd << "\n";
    return nullptr;
}

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9034);
    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }
    if (listen(listen_fd, 10) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    reactor_ptr = startReactor();
    addFdToReactor(reactor_ptr, listen_fd, acceptCallback);

    std::cout << "Server running on port 9034\n";
    while (true) {
        sleep(1);
    }

    // לא יגיע לכאן בדרך כלל, אבל אפשר להוסיף ניקוי בסיום
    stopReactor(reactor_ptr);
    close(listen_fd);
    return 0;
}