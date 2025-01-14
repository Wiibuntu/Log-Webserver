#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <array>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>

#define PORT 28885
#define LOG_FILE "/home/nick/Log-Webserver/log.txt"  // Replace with the absolute path to your log file
#define PID_FILE "/var/run/webserver.pid"  // Path to PID file

// Function to log errors
void log_error(const std::string &message) {
    std::cerr << "Error: " << message << std::endl;
}

// Function to handle termination signals
void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "Shutting down webserver...\n";
        unlink(PID_FILE); // Remove the PID file
        exit(0);
    }
}

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

    int return_code = pclose(pipe);
    if (return_code != 0) {
        temps = "Error: Sensors command failed.";
    }

    if (temps.empty()) {
        temps = "No temperature data available.";
    }

    return temps;
}

// Function to read the log file
std::vector<std::string> read_log_file() {
    std::vector<std::string> log_lines;
    std::ifstream log_file(LOG_FILE);
    if (!log_file) {
        log_lines.push_back("Error: Unable to read log file.");
        return log_lines;
    }

    std::string line;
    while (std::getline(log_file, line)) {
        log_lines.push_back(line);
    }

    log_file.close();
    return log_lines;
}

// Function to create the HTML response
std::string create_html_response(const std::vector<std::string> &log_lines, const std::string &system_temps) {
    std::ostringstream response;

    // HTML and CSS structure
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n";
    response << "Pragma: no-cache\r\n";
    response << "Expires: -1\r\n\r\n";

    response << "<!DOCTYPE html><html><head><style>";
    response << "body { background-color: black; color: white; font-family: monospace; font-size: 16px; margin: 0; padding: 0; }";
    response << ".widget { border: 1px solid white; margin: 10px; padding: 10px; border-radius: 8px; background-color: rgba(255, 255, 255, 0.1); }";
    response << ".widget-header { font-weight: bold; margin-bottom: 10px; font-size: 18px; }";
    response << ".widget-content { max-height: 300px; overflow-y: auto; }";
    response << "</style>";
    response << "<meta http-equiv=\"refresh\" content=\"5\">"; // Refresh every 5 seconds
    response << "</head><body>";

    // System Temperature Widget
    response << "<div class=\"widget\">";
    response << "<div class=\"widget-header\">System Temperatures</div>";
    response << "<div class=\"widget-content\">";
    std::istringstream temps_stream(system_temps);
    std::string line;
    while (std::getline(temps_stream, line)) {
        response << line << "<br>";
    }
    response << "</div></div>";

    // Log File Widget
    response << "<div class=\"widget\">";
    response << "<div class=\"widget-header\">Log File</div>";
    response << "<div class=\"widget-content\">";
    for (const auto &log_line : log_lines) {
        response << log_line << "<br>";
    }
    response << "</div></div>";

    response << "</body></html>";
    return response.str();
}

// Function to serve client requests
void serve_client(int client_socket) {
    std::vector<std::string> log_lines = read_log_file();
    std::string system_temps = get_system_temps();
    std::string response = create_html_response(log_lines, system_temps);
    send(client_socket, response.c_str(), response.size(), 0);
    close(client_socket);
}

// Function to start the webserver
void start_server() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        log_error("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    // Configure socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        log_error("Failed to set socket options.");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        log_error("Bind failed. Port " + std::to_string(PORT) + " may already be in use.");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        log_error("Listen failed.");
        exit(EXIT_FAILURE);
    }

    std::cout << "Webserver running on port " << PORT << "...\n";

    // Accept and handle connections
    while (true) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_socket < 0) {
            log_error("Failed to accept connection.");
            continue;
        }
        std::thread(serve_client, client_socket).detach();
    }
}

// Main function
int main() {
    // Handle termination signals to clean up the PID file
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Start the webserver
    start_server();

    return 0;
}

