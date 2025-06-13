#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "Failed to create socket\n";
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(9034);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr); // או כתובת של שרת אחר

    if (connect(sock, (sockaddr*)&server, sizeof(server)) < 0) {
        cerr << "Failed to connect to server\n";
        return 1;
    }

    cout << "Connected to server. Type commands:\n";

    string line;
    char buffer[4096];

    while (getline(cin, line)) {
        line += '\n'; // חייבים שליחה עם שורת סיום
        send(sock, line.c_str(), line.size(), 0);

        // קבלה של תגובה מהשרת
        ssize_t bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            cerr << "Server closed connection or error\n";
            break;
        }

        buffer[bytesReceived] = 0;
        cout << "Server: " << buffer;
    }

    close(sock);
    return 0;
}
