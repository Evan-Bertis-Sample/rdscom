#!/usr/bin/env bash

# Gather executables in the build/ folder
exes=( $(find ./build -maxdepth 1 -type f -executable) )

# Print the list of executables
echo "Select an executable to run:"
i=1
for exe in "${exes[@]}"; do
    echo "$i) $exe"
    ((i++))
done

# if there are no executables, exit
if [[ ${#exes[@]} -eq 0 ]]; then
    echo "No executables found."
    exit 1
fi

# if there is only one executable, run it
if [[ ${#exes[@]} -eq 1 ]]; then
    "${exes[0]}"
    exit 0
fi

# Get user's choice
read -p "Enter a number: " choice

# Run the selected executable
if [[ $choice -ge 1 && $choice -le ${#exes[@]} ]]; then
    "${exes[$choice-1]}"
else
    echo "Invalid choice."
fi