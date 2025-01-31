#!/usr/bin/env bash

mkdir -p build
for proj in examples/*.cpp
do
    echo "Building $proj"
    g++ -std=c++11 -Wall -Wextra -Wpedantic -Werror -o build/$(basename $proj .cpp) $proj -I.
done