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

// Compute Convex Hull using Monotone Chain Algorithm
vector<Point> convexHull_vector(vector<Point> &points) {
    int n = points.size(), k = 0;
    if (n <= 3) return points;

    sort(points.begin(), points.end());
    vector<Point> hull(2*n);

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

    hull.resize(k-1); // Remove last point
    return hull;
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
    int numPoints;
    if (!(cin >> numPoints) || numPoints <= 0) {
        cerr << "Error: Invalid number of points." << endl;
        return 1;
    }
    cin.ignore(); 

    vector<Point> points;
    for (int i = 0; i < numPoints; ++i) {
        string line;
        if (!getline(cin, line)) {
            cerr << "Error: Not enough input lines." << endl;
            return 1;
        }

        replace(line.begin(), line.end(), ',', ' ');

        stringstream ss(line);
        Point p;
        if (!(ss >> p.x >> p.y)) {
            cerr << "Error: Invalid point format at line " << i + 1 << "." << endl;
            return 1;
        }

        string extra;
        if (ss >> extra) {
            cerr << "Error: Extra data in line " << i + 1 << "." << endl;
            return 1;
        }

        points.push_back(p);
    }

    auto start1 = chrono::high_resolution_clock::now();
    auto hull1 = convexHullDeque(points);
    auto end1 = chrono::high_resolution_clock::now();
    chrono::duration<double> diff1 = end1 - start1;

    auto start2 = chrono::high_resolution_clock::now();
    auto hull2 = convexHull_vector(points);
    auto end2 = chrono::high_resolution_clock::now();
    chrono::duration<double> diff2 = end2 - start2;


    float area1 = polygonArea(hull1);
    cout << "Deque: " << area1  << "    Duration: " << diff1.count() << " s\n";

    float area2 = polygonArea(hull2);
    cout << "Vector: " << area2 << "   Duration: " << diff2.count() << " s\n";

    return 0;
}
