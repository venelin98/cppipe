#!/bin/sh
set -e

# Test cppipe functions
cppipe test/test.cpp

# Test on a C file
echo OK | cppipe test/c_file.c

echo ALL TESTS PASSED
