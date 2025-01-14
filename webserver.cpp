#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 28885
#define LOG_FILE "/path/to/log.txt"  // Replace with the absolute path to your log file
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

// Function to check if the webserver is already running
bool is_webserver_running() {
    std::ifstream pid_file(PID_FILE);
    if (!pid_file.is_open()) {
        return false;  // PID file does not exist
    }

    int pid;
    pid_file >> pid;
    pid_file.close();

    // Check if the process with this PID exists
    if (kill(pid, 0) == 0) {
        return true;  // Process is running
    } else {
        unlink(PID_FILE);  // Remove stale PID file
        return false;
    }
}

// Function to write the current process PID to the PID file
void write_pid_file() {
    std::ofstream pid_file(PID_FILE, std::ios::trunc);
    if (!pid_file.is_open()) {
        log_error("Unable to write to PID file.");
        exit(EXIT_FAILURE);
    }
    pid_file << getpid() << std::endl;
    pid_file.close();
}

// Function to stop the existing webserver
void stop_existing_webserver() {
    std::ifstream pid_file(PID_FILE);
    if (!pid_file.is_open()) {
        return;  // No PID file to read
    }

    int pid;
    pid_file >> pid;
    pid_file.close();

    if (kill(pid, SIGTERM) == 0) {
        std::cout << "Stopped existing webserver (PID: " << pid << ").\n";
    } else {
        log_error("Failed to stop existing webserver.");
    }

    unlink(PID_FILE);  // Remove PID file
}

// Function to serve client requests
void serve_client(int client_socket) {
    std::ostringstream response;

    // Create a simple HTML response
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/html\r\n\r\n";
    response << "<!DOCTYPE html><html><head><title>Webserver</title></head><body>";
    response << "<h1>Webserver is running</h1>";
    response << "</body></html>";

    send(client_socket, response.str().c_str(), response.str().size(), 0);
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
        log_error("Bind failed.");
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
    // Check if the webserver is already running
    if (is_webserver_running()) {
        std::cout << "Webserver is already running. Stopping it...\n";
        stop_existing_webserver();
    }

    // Write the current process PID to the PID file
    write_pid_file();

    // Handle termination signals to clean up the PID file
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Start the webserver
    start_server();

    return 0;
}

