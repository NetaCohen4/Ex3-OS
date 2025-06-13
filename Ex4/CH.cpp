#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <deque>
#include <chrono>
#include <cstring>
#include <string>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>

using namespace std;
struct Point { 
    float x, y; 
    bool operator<(const Point &o) const { 
        return x < o.x || (x==o.x && y<o.y); 
    } 
};

float cross(const Point &O, const Point &A, const Point &B) {
    return (A.x - O.x)*(B.y - O.y) - (A.y - O.y)*(B.x - O.x);
}

vector<Point> convexHullDeque(vector<Point> P) {
    int n = P.size();
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

float polygonArea(const vector<Point> &poly) {
    float area = 0;
    int n = poly.size();
    for (int i = 0; i < n; ++i) {
        area += poly[i].x * poly[(i+1)%n].y - poly[i].y * poly[(i+1)%n].x;
    }
    return abs(area) / 2.0;
}

// הגרף ה"גלובלי"
vector<Point> points;
string handleCommand(const string& line) {
    stringstream ss(line);
    string command;
    ss >> command;
    stringstream response;

    if (command == "Newpoint") {
        float x, y;
        ss >> x >> y;
        points.push_back({x, y});
        response << "OK: Added point\n";
    }
    else if (command == "Removepoint") {
        float x, y;
        ss >> x >> y;
        auto it = remove_if(points.begin(), points.end(), [x, y](const Point& p) {
            return p.x == x && p.y == y;
        });
        points.erase(it, points.end());
        response << "OK: Removed point\n";
    }
    else if (command == "CH") {
        auto hull = convexHullDeque(points);
        float area = polygonArea(hull);
        response << "Convex Hull Area: " << area << "\n";
    }
    else {
        response << "ERROR: Unknown command\n";
    }

    return response.str();
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9034);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_socket, 5) < 0) {
        perror("listen");
        return 1;
    }

    cout << "Server listening on port 9034...\n";

    fd_set master_fds, read_fds;
    FD_ZERO(&master_fds);
    FD_ZERO(&read_fds);

    FD_SET(server_socket, &master_fds);
    int fd_max = server_socket;

    while (true) {
        read_fds = master_fds;

        if (select(fd_max + 1, &read_fds, nullptr, nullptr, nullptr) == -1) {
            perror("select");
            return 1;
        }

        for (int i = 0; i <= fd_max; ++i) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == server_socket) {
                    // חיבור חדש
                    sockaddr_in client_addr;
                    socklen_t addrlen = sizeof(client_addr);
                    int new_fd = accept(server_socket, (sockaddr*)&client_addr, &addrlen);
                    if (new_fd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(new_fd, &master_fds);
                        if (new_fd > fd_max) {
                            fd_max = new_fd;
                        }
                        cout << "New client connected (fd=" << new_fd << ")\n";
                    }
                } else {
                    // קריאה מלקוח קיים
                    char buf[1024];
                    int nbytes = recv(i, buf, sizeof(buf)-1, 0);
                    if (nbytes <= 0) {
                        if (nbytes == 0) {
                            cout << "Client (fd=" << i << ") disconnected.\n";
                        } else {
                            perror("recv");
                        }
                        close(i);
                        FD_CLR(i, &master_fds);
                    } else {
                        buf[nbytes] = '\0';
                        stringstream ss(buf);
                        string command;
                        ss >> command;
                        /*
                        אם הפקודה היא "Newgraph", נקרא את מספר הנקודות ולאחר מכן נקרא את כל הנקודות
                        מהלקוח. אם הפקודה היא אחרת, נשתמש בפונקציה handleCommand כדי לטפל בה.
                        */
                        if (command == "Newgraph") {
                            int n;
                            ss >> n;
                            points.clear();
                            for (int j = 0; j < n; ++j) {
                                if (int nbytes = recv(i, buf, sizeof(buf)-1, 0); nbytes <= 0) {
                                    if (nbytes == 0) {
                                        cout << "Client (fd=" << i << ") disconnected.\n";
                                    } else {
                                        perror("recv");
                                    }
                                    close(i);
                                    cerr << "Error: Expected " << n << " point lines.\n";
                                    break;
                                }
                                replace(buf, buf + strlen(buf), ',', ' ');
                                stringstream pointStream(buf);
                                Point p;
                                pointStream >> p.x >> p.y;
                                points.push_back(p);
                            }
                            response << "OK: Send " << n << " points\n";
                        }
    else 
                        string response = handleCommand(buf);
                        send(i, response.c_str(), response.length(), 0);
                    }
                }
            }
        }
    }

    return 0;
}

