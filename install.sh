#!/bin/sh
set -e
cd src

# Install location
PREFIX=/usr/local


# Compile cppipe
options="-std=c++17 -Ofast --whole-program -march=native -DNDEBUG -Wall -Wextra -Wno-parentheses -Wno-unused-result -s -pipe"
# options="-std=c++17 -g"
g++ -o cppipe $options cppipe.cpp
chmod 755 cppipe
mv cppipe ${PREFIX}/bin

# Install headers
mkdir -p ${PREFIX}/include/cppipe
cp basicTypes.h commands.hpp commands.inl childProcess.hpp childProcess.inl ${PREFIX}/include/cppipe

# Clear old cache
rm -rf ~/.cache/cppipe ${XDG_CACHE_HOME}/cppipe
