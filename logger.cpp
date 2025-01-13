#include <iostream>
#include <fstream>
#include <string>

#define LOG_FILE "log.txt"

void log_message(const std::string &message) {
    std::ofstream log_file(LOG_FILE, std::ios::app);
    if (!log_file) {
        std::cerr << "Failed to open log file\n";
        return;
    }
    log_file << message << std::endl;
    log_file.close();
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        // Log all command-line arguments as a single message
        std::string message;
        for (int i = 1; i < argc; ++i) {
            message += argv[i];
            if (i < argc - 1) {
                message += " ";
            }
        }
        log_message(message);
        return 0;
    }

    // Interactive mode
    std::string input;
    std::cout << "Logger started. Type messages to log. Type 'exit' to quit.\n";
    while (true) {
        std::getline(std::cin, input);
        if (input == "exit") {
            break;
        }
        log_message(input);
    }
    return 0;
}
