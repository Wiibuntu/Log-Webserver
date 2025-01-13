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

#define PORT 28885
#define LOG_FILE "log.txt"

// Function to fetch system temperatures using lm-sensors
std::string get_system_temps() {
    std::string temps;
    std::array<char, 128> buffer;
    FILE *pipe = popen("sensors", "r");
    if (!pipe) {
        return "Error: Unable to fetch temperatures";
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        temps += buffer.data();
    }
    pclose(pipe);
    return temps;
}

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

    // Fetch the system temperatures
    std::string system_temps = get_system_temps();

    // Build HTML response
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/html\r\n\r\n";
    response << "<!DOCTYPE html><html><head><style>";
    response << "body { background-color: black; color: white; font-family: monospace; font-size: 18px; margin: 0; padding: 0; }";
    response << "#log { padding: 20px; }";
    response << "#temps { position: fixed; top: 10px; right: 10px; background: rgba(255, 255, 255, 0.1); padding: 10px; border-radius: 8px; }";
    response << "</style>";
    response << "<meta http-equiv=\"refresh\" content=\"2\">";  // Auto-refresh every 2 seconds
    response << "</head><body>";

    // Display the system temperatures in a box
    response << "<div id=\"temps\">";
    response << "<strong>System Temperatures:</strong><br>";
    std::istringstream temps_stream(system_temps);
    while (std::getline(temps_stream, line)) {
        response << line << "<br>";
    }
    response << "</div>";

    // Add the log lines to the HTML
    response << "<div id=\"log\">";
    for (const auto &log_line : log_lines) {
        response << log_line << "<br>";
    }
    response << "</div>";

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

    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main() {
    daemonize(); // Run the program as a daemon
    start_server();
    return 0;
}

