#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define LOG_FILE "/home/nick/Log-Webserver/log.txt"  // Replace with the absolute path to your log file
#define LOGGER_PORT 3000            // Port number for the logger

// Function to log messages to the log file
void log_message(const std::string &message) {
    std::ofstream log_file(LOG_FILE, std::ios::app);
    if (!log_file) {
        std::cerr << "Error: Unable to open log file." << std::endl;
        return;
    }
    log_file << message << std::endl;
    log_file.close();
    std::cout << "Logged: " << message << std::endl;
}

// Function to handle incoming client connections
void handle_client(int client_socket) {
    char buffer[1024] = {0};
    int bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        std::string message(buffer);
        log_message(message);  // Log the received message
    }

    close(client_socket);
}

// Function to start the logger server
void start_logger_server() {
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
    address.sin_port = htons(LOGGER_PORT);

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

    std::cout << "Logger server is running on port " << LOGGER_PORT << "..." << std::endl;

    // Accept and handle connections
    while (true) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }
        std::thread(handle_client, client_socket).detach();
    }
}

// Function to run the logger in local mode
void run_local_logger() {
    std::string input;
    std::cout << "Logger started in local mode. Type messages to log. Type 'exit' to quit." << std::endl;

    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);

        if (input == "exit") {
            break;
        }

        if (!input.empty()) {
            log_message(input);  // Log the user input
        } else {
            std::cout << "No input provided. Please type a message to log." << std::endl;
        }
    }
}

// Main function
int main() {
    std::string mode;

    std::cout << "Select mode: [local / remote] ";
    std::getline(std::cin, mode);

    if (mode == "local") {
        run_local_logger();
    } else if (mode == "remote") {
        start_logger_server();
    } else {
        std::cerr << "Invalid mode selected. Use 'local' for local logging or 'remote' for remote logging." << std::endl;
        return 1;
    }

    return 0;
}

