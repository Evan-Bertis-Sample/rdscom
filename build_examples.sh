#!/usr/bin/env bash

mkdir -p build
for proj in examples/*.cpp
do
    echo "Building $proj"
    gcc -Wall -I. -o "build/$(basename "$proj" .c)" "$proj"
done