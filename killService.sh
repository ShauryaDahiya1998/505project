#!/bin/bash

# File where PIDs are stored
pid_file="pids.txt"

# Ensure the PID file exists
if [ ! -f "$pid_file" ]; then
    echo "PID file not found."
    exit 1
fi

# Read PIDs from the file and kill each process
while read pid; do
    kill $pid
    echo "Killed process $pid."
done < "$pid_file"

# Optionally, remove the PID file after killing the processes
rm "$pid_file"
echo "All processes have been terminated."
