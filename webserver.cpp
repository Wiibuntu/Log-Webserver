#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <sstream>
#include <vector>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 28885
#define LOG_FILE "log.txt"

void serve_client(int client_socket) {
    std::ifstream log_file(LOG_FILE);
    std::ostringstream response;

    // Read the log file into a vector for reversing
    std::vector<std::string> log_lines;
    std::string line;
    while (std::getline(log_file, line)) {
        log_lines.push_back(line);
    }
    log_file.close();

    // Reverse the log entries to show the newest first
    std::reverse(log_lines.begin(), log_lines.end());

    // Build HTML response
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/html\r\n\r\n";
    response << "<!DOCTYPE html><html><head><style>";
    response << "body { background-color: black; color: white; font-family: monospace; font-size: 18px; }"; // Increased font size
    response << "</style>";
    response << "<meta http-equiv=\"refresh\" content=\"2\">"; // Auto-refresh every 2 seconds
    response << "</head><body>";

    // Add the log lines to the HTML
    for (const auto &log_line : log_lines) {
        response << log_line << "<br>";
    }

    response << "</body></html>";

    // Send response
    std::string response_str = response.str();
    send(client_socket, response_str.c_str(), response_str.size(), 0);
    close(client_socket);
}

void start_server() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Web server running on port " << PORT << "...\n";

    // Accept and handle connections
    while (true) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_socket < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }
        std::thread(serve_client, client_socket).detach();
    }
}

int main() {
    start_server();
    return 0;
}
