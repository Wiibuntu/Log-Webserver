#!/bin/bash

# Clone and Compile Recent Webserver

# Define variables
REPO_URL="https://github.com/Wiibuntu/Log-Webserver.git"
TARGET_DIR="Log-Webserver"

# Check if the directory exists
if [ -d "$TARGET_DIR" ]; then
    echo "Directory '$TARGET_DIR' exists. Pulling latest changes..."
    cd "$TARGET_DIR"
    git pull origin
else
    echo "Directory '$TARGET_DIR' does not exist. Cloning repository..."
    git clone "$REPO_URL"
    cd "$TARGET_DIR"
fi

echo "Operation complete."

g++ -o webserver webserver.cpp

g++ -o logger logger.cpp

sudo ./webserver

