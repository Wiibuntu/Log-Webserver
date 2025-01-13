#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <array>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 28885
#define LOG_FILE "/path/to/log.txt" // Use an absolute path for the log file

// Function to fetch system temperatures using lm-sensors
std::string get_system_temps() {
    std::string temps;
    std::array<char, 128> buffer;
    FILE *pipe = popen("sensors", "r");
    if (!pipe) {
        return "Error: Unable to fetch temperatures.";
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        temps += buffer.data();
    }
    pclose(pipe);

    if (temps.empty()) {
        return "No temperature data available.";
    }

    return temps;
}

// Function to serve client requests
void serve_client(int client_socket) {
    std::ifstream log_file(LOG_FILE);
    std::ostringstream response;

    if (!log_file) {
        response << "HTTP/1.1 500 Internal Server Error\r\n";
        response << "Content-Type: text/html\r\n";
        response << "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n";
        response << "Pragma: no-cache\r\n";
        response << "Expires: -1\r\n\r\n";
        response << "<!DOCTYPE html><html><head><style>";
        response << "body { background-color: black; color: white; font-family: monospace; font-size: 18px; }";
        response << "</style></head><body>";
        response << "<h1>Error: Unable to read log file.</h1>";
        response << "</body></html>";
        send(client_socket, response.str().c_str(), response.str().size(), 0);
        close(client_socket);
        return;
    }

    // Read the log file into a vector for reversing
    std::vector<std::string> log_lines;
    std::string line;
    while (std::getline(log_file, line)) {
        log_lines.push_back(line);
    }
    log_file.close();

    // Reverse the log entries to show the newest first
    std::reverse(log_lines.begin(), log_lines.end());

    // Build HTML response with caching disabled
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n";
    response << "Pragma: no-cache\r\n";
    response << "Expires: -1\r\n\r\n";
    response << "<!DOCTYPE html><html><head><style>";
    response << "body { background-color: black; color: white; font-family: monospace; font-size: 18px; }";
    response << "</style>";
    response << "<meta http-equiv=\"refresh\" content=\"2\">"; // Auto-refresh every 2 seconds
    response << "</head><body>";

    // Add the log lines to the HTML
    for (const auto &log_line : log_lines) {
        response << log_line << "<br>";
    }

    response << "</body></html>";

    // Send response
    send(client_socket, response.str().c_str(), response.str().size(), 0);
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
            continue;
        }
        std::thread(serve_client, client_socket).detach();
    }
}

void daemonize() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        // Parent process exits, child continues
        std::cout << "Daemon process started with PID: " << pid << std::endl;
        exit(0);
    }

    // Child process becomes a daemon
    umask(0); // Set file mode creation mask
    if (setsid() < 0) {
        perror("Failed to create a new session");
        exit(EXIT_FAILURE);
    }

    if (chdir("/") < 0) { // Change working directory to root
        perror("Failed to change directory to /");
        exit(EXIT_FAILURE);
    }

    // Redirect standard file descriptors to /dev/null
    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null == -1) {
        perror("Failed to open /dev/null");
        exit(EXIT_FAILURE);
    }

    dup2(dev_null, STDIN_FILENO);
    dup2(dev_null, STDOUT_FILENO);
    dup2(dev_null, STDERR_FILENO);
    close(dev_null);
}

int main() {
    daemonize(); // Run the program as a daemon
    start_server();
    return 0;
}

