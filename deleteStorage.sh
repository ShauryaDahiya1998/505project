#!/bin/bash

# Define the directory containing the storage directories
base_directory="/home/cis5050/505project/"
# Loop through each storage directory and delete _log.txt files
for storage in "$base_directory"/storage*; do
    if [ -d "$storage" ]; then  # Check if it's a directory
        echo "Deleting _log.txt files in $storage"
        rm "$storage"/*_log.txt
    fi
done

echo "Deletion complete."
