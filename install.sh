#!/bin/sh
set -e
cd src

# Install location
PREFIX=/usr/local

# Compile cppipe
options="-std=c++17 -Ofast -fwhole-program -march=native -DNDEBUG -Wall -Wextra -Wno-parentheses -Wno-unused-result -s -pipe"
# options="-std=c++17 -g"
c++ -o cppipe $options cppipe.cpp
chmod 755 cppipe
mv cppipe ${PREFIX}/bin

# Install headers
mkdir -p ${PREFIX}/include/cppipe
cp basicTypes.h commands.hpp commands.inl childProcess.hpp childProcess.inl ${PREFIX}/include/cppipe
chmod 755 ${PREFIX}/include/cppipe
chmod 644 ${PREFIX}/include/cppipe/*

# Clear old cache
rm -rf ~/.cache/cppipe ${XDG_CACHE_HOME}/cppipe
