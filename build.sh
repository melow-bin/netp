#!/bin/bash
CXX="g++"
CXXFLAGS="-Wall -Wextra -std=c++20"
TARGET=netp

SRCS=netp.cpp
OBJS=${SRCS/.cpp/.o}

build() {
    echo "Building $TARGET..."
    $CXX $OBJS -o $TARGET
    echo "$TARGET built successfully."
}

compile() {
    echo "Compiling $SRCS..."
    $CXX $CXXFLAGS -c $SRCS
}

clean() {
    echo "Cleaning up..."
    rm -f $OBJS $TARGET
    echo "Cleaned up successfully."
}

if [ "$1" == "clean" ]; then
    clean
else
    compile
    build
fi

