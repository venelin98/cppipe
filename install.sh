#!/bin/sh
set -e

# Compile lib
options="-std=c++17 -Ofast -flto -march=native -DNDEBUG -Wall -Wextra -Wno-parentheses -Wno-unused-result -s -pipe"
g++ -c $options commands.cpp childProcess.cpp

# Create lib
ar r libcppipe.a commands.o childProcess.o
mv libcppipe.a /usr/local/lib
rm commands.o childProcess.o

# Compile cppipe
g++ -o cppipe $options cppipe.cpp -lcppipe
chmod 755 cppipe
mv cppipe /usr/local/bin

# Add headers
mkdir -p /usr/local/include/cppipe
cp basicTypes.h commands.hpp childProcess.hpp /usr/local/include/cppipe
