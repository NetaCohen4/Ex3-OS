#include "../Ex5/Reactor.hpp"
#include <vector>
#include <set>
#include <map>
#include <string>
#include <sstream>
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

#define PORT 9034

void* reactor = startReactor();

// ------------------ מבנה נתונים לגרף ------------------
struct Point {
    float x, y;
    bool operator<(const Point& other) const {
        if (x != other.x) return x < other.x;
        return y < other.y;
    }
};

std::set<Point> graph;

// ------------------ פונקציות עזר ------------------
float cross(const Point& O, const Point& A, const Point& B) {
    return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
}

std::vector<Point> convex_hull(const std::set<Point>& pts) {
    std::vector<Point> points(pts.begin(), pts.end()), hull;
    int n = points.size(), k = 0;
    if (n == 0) return hull;
    hull.resize(2 * n);
    std::sort(points.begin(), points.end(), [](const Point& a, const Point& b) {
        return (a.x < b.x) || (a.x == b.x && a.y < b.y);
    });
    // Lower hull
    for (int i = 0; i < n; ++i) {
        while (k >= 2 && cross(hull[k-2], hull[k-1], points[i]) <= 0) k--;
        hull[k++] = points[i];
    }
    // Upper hull
    for (int i = n-2, t = k+1; i >= 0; --i) {
        while (k >= t && cross(hull[k-2], hull[k-1], points[i]) <= 0) k--;
        hull[k++] = points[i];
    }
    hull.resize(k-1);
    return hull;
}

float polygon_area(const std::vector<Point>& hull) {
    float area = 0;
    int n = hull.size();
    for (int i = 0; i < n; ++i) {
        area += hull[i].x * hull[(i+1)%n].y - hull[(i+1)%n].x * hull[i].y;
    }
    return std::abs(area) / 2.0f;
}

// ------------------ עיבוד פקודות ------------------
void process_command(const std::string& cmd, int client_fd) {
    std::istringstream iss(cmd);
    std::string op;
    iss >> op;
    if (op == "Newgraph") {
        int n; iss >> n;
        graph.clear();
        for (int i = 0; i < n; ++i) {
            std::string line;
            std::getline(iss, line); // אם יש בשורה, אחרת תקרא מהקלט
            if (line.empty()) std::getline(std::cin, line);
            std::istringstream lss(line);
            float x, y; char comma;
            lss >> x >> comma >> y;
            graph.insert({x, y});
        }
        std::string resp = "OK\n";
        send(client_fd, resp.c_str(), resp.size(), 0);
    }
    else if (op == "Newpoint") {
        float x, y; char comma;
        iss >> x >> comma >> y;
        graph.insert({x, y});
        std::string resp = "OK\n";
        send(client_fd, resp.c_str(), resp.size(), 0);
    }
    else if (op == "Removepoint") {
        float x, y; char comma;
        iss >> x >> comma >> y;
        graph.erase({x, y});
        std::string resp = "OK\n";
        send(client_fd, resp.c_str(), resp.size(), 0);
    }
    else if (op == "CH") {
        auto hull = convex_hull(graph);
        float area = polygon_area(hull);
        std::ostringstream oss;
        oss << area << "\n";
        std::string resp = oss.str();
        send(client_fd, resp.c_str(), resp.size(), 0);
    }
    else {
        std::string resp = "Unknown command\n";
        send(client_fd, resp.c_str(), resp.size(), 0);
    }
}

// ------------------ callback ללקוח ------------------
void* client_callback(int fd) {
    char buf[1024] = {0};
    int bytes = recv(fd, buf, sizeof(buf)-1, 0);
    if (bytes <= 0) {
        close(fd);
        // הסר fd מה־reactor (צריך גישה ל־reactor)
        // removeFdFromReactor(reactor, fd);
        return nullptr;
    }
    std::string cmd(buf);
    cmd.erase(std::remove(cmd.begin(), cmd.end(), '\r'), cmd.end());
    cmd.erase(std::remove(cmd.begin(), cmd.end(), '\n'), cmd.end());
    process_command(cmd, fd);
    return nullptr;
}

// ------------------ callback ל־accept ------------------
void* accept_callback(int listen_fd) {
    sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &addrlen);
    if (client_fd >= 0) {
        // הוסף את הלקוח החדש ל־reactor
        addFdToReactor(reactor, client_fd, client_callback);
    }
    return nullptr;
}

// ------------------ main ------------------
int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    bind(listen_fd, (sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 10);

    addFdToReactor(reactor, listen_fd, accept_callback);

    // לולאה ראשית - ה־reactor רץ ב־thread משלו
    while (true) {
        sleep(1);
    }
    stopReactor(reactor);
    close(listen_fd);
    return 0;
}