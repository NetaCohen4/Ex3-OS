#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <deque>
#include <chrono>

using namespace std;

struct Point {
    float x, y;

    bool operator<(const Point &other) const {
        return x < other.x || (x == other.x && y < other.y);
    }
};

// Cross product
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

// Calculate polygon area
float polygonArea(const vector<Point> &poly) {
    float area = 0;
    int n = poly.size();
    for (int i = 0; i < n; ++i) {
        area += poly[i].x * poly[(i+1)%n].y - poly[i].y * poly[(i+1)%n].x;
    }
    return abs(area) / 2.0;
}

int main() {
    vector<Point> points;
    string line;

    while (getline(cin, line)) {
        stringstream ss(line);
        string command;
        ss >> command;

        if (command == "Newgraph") {
            int n;
            ss >> n;
            points.clear();

            for (int i = 0; i < n; ++i) {
                if (!getline(cin, line)) {
                    cerr << "Error: Expected " << n << " point lines." << endl;
                    return 1;
                }

                replace(line.begin(), line.end(), ',', ' ');
                stringstream pointStream(line);
                Point p;
                pointStream >> p.x >> p.y;
                points.push_back(p);
            }
        } else if (command == "CH") {
            auto hull1 = convexHullDeque(points);

            float area1 = polygonArea(hull1);
            cout << area1 << endl;

        } else if (command == "Newpoint") {
            float x, y;
            ss >> x >> y;
            points.push_back({x, y});
        } else if (command == "Removepoint") {
            float x, y;
            ss >> x >> y;
            auto it = remove_if(points.begin(), points.end(), [x, y](const Point& p) {
                return p.x == x && p.y == y;
            });
            if (it != points.end()) {
                points.erase(it, points.end());
            }
        }
    }

    return 0;
}