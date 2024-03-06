#!/bin/sh
set -e

# Install location
PREFIX=/usr/local

# Compile lib
options="-std=c++17 -Ofast -flto -march=native -DNDEBUG -Wall -Wextra -Wno-parentheses -Wno-unused-result -s -pipe"
g++ -c $options commands.cpp childProcess.cpp

# Create lib
ar rc libcppipe.a commands.o childProcess.o
mv libcppipe.a ${PREFIX}/lib
rm commands.o childProcess.o

# Compile cppipe
g++ -o cppipe $options cppipe.cpp -lcppipe
chmod 755 cppipe
mv cppipe ${PREFIX}/bin

# Install headers
mkdir -p ${PREFIX}/include/cppipe
cp basicTypes.h commands.hpp childProcess.hpp ${PREFIX}/include/cppipe
