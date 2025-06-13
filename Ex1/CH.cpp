#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <sstream>

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
vector<Point> convexHull(vector<Point> &points) {
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

    vector<Point> hull = convexHull(points);
    float area = polygonArea(hull);

    cout << area << endl;
    return 0;
}
