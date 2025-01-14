#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <thread>
#include <array>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 28885
#define LOG_FILE "/path/to/log.txt" // Replace with the absolute path to the log file

// Function to log errors to stderr
void log_error(const std::string &message) {
    std::cerr << "Error: " << message << std::endl;
}

// Function to fetch system temperatures using lm-sensors
std::string get_system_temps() {
    std::string temps;
    std::array<char, 128> buffer;
    FILE *pipe = popen("sensors", "r");
    if (!pipe) {
        return "Error: Unable to execute sensors command.";
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
        log_error("Unable to open log file.");
        return log_lines;
    }

    std::string line;
    while (std::getline(log_file, line)) {
        log_lines.push_back(line);
    }

    log_file.close();
    return log_lines;
}

// Function to create the HTTP response
std::string create_http_response(const std::vector<std::string> &log_lines, const std::string &system_temps) {
    std::ostringstream response;

    // HTTP headers
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n";
    response << "Pragma: no-cache\r\n";
    response << "Expires: -1\r\n\r\n";

    // HTML content
    response << "<!DOCTYPE html><html><head><style>";
    response << "body { background-color: black; color: white; font-family: monospace; font-size: 18px; margin: 0; padding: 0; }";
    response << "#temps { position: fixed; top: 10px; right: 10px; background: rgba(255, 255, 255, 0.1); padding: 10px; border-radius: 8px; }";
    response << "</style>";
    response << "<meta http-equiv=\"refresh\" content=\"2\">"; // Auto-refresh every 2 seconds
    response << "</head><body>";

    // System temperatures
    response << "<div id=\"temps\">";
    response << "<strong>System Temperatures:</strong><br>";
    std::istringstream temps_stream(system_temps);
    std::string line;
    while (std::getline(temps_stream, line)) {
        response << line << "<br>";
    }
    response << "</div>";

    // Log entries
    response << "<div id=\"log\">";
    for (const auto &log_line : log_lines) {
        response << log_line << "<br>";
    }
    response << "</div>";

    response << "</body></html>";
    return response.str();
}

// Function to serve client requests
void serve_client(int client_socket) {
    // Read log file and system temperatures
    std::vector<std::string> log_lines = read_log_file();
    std::string system_temps = get_system_temps();

    // Reverse the log entries to show the newest first
    std::reverse(log_lines.begin(), log_lines.end());

    // Create and send the HTTP response
    std::string response = create_http_response(log_lines, system_temps);
    send(client_socket, response.c_str(), response.size(), 0);
    close(client_socket);
}

// Function to start the web server
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
        log_error("Bind failed.");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        log_error("Listen failed.");
        exit(EXIT_FAILURE);
    }

    std::cout << "Web server running on port " << PORT << "...\n";

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

// Function to daemonize the server
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

