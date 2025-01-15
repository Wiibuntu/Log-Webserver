#!/bin/bash

# Remote logger script
# Usage: ./remote_logger.sh <server_ip> <message>

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <server_ip> <message>"
    exit 1
fi

SERVER_IP=$1
MESSAGE=$2
LOGGER_PORT=3000

# Send the log message to the remote logger
echo -n "$MESSAGE" | nc "$SERVER_IP" "$LOGGER_PORT"

if [ $? -eq 0 ]; then
    echo "Message sent successfully to $SERVER_IP:$LOGGER_PORT"
else
    echo "Failed to send message to $SERVER_IP:$LOGGER_PORT"
fi
