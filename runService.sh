#!/bin/bash

# File where PIDs will be stored
pid_file="pids.txt"

# Starting each command and storing their PIDs
./kvsCoord -p 8000 storage &
echo $! > "$pid_file"
./kvs -p 8001 0 storage1/ &
echo $! >> "$pid_file"
./kvs -p 8002 1 storage2/ &
echo $! >> "$pid_file"
./kvs -p 8003 2 storage3/ &
echo $! >> "$pid_file"
./kvs -p 8004 3 storage4/ &
echo $! >> "$pid_file"
./myserver -p 9000 &
echo $! >> "$pid_file"
./myserver -p 9001 &
echo $! >> "$pid_file"
./frontEndCoord &
echo $! >> "$pid_file"

echo "All processes started and PIDs stored in $pid_file."