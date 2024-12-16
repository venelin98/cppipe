#!/bin/sh
set -e

# Test cppipe functions
OKs=$(cppipe test/functions_test.cppipe 2>/dev/null | grep OK | wc -l)
if ! [ $OKs = 13 ]
then
    echo "Functions test failed: EXPECTED 13 OKs, got $OKs"
    exit 1
fi
echo Functions test OK!

# Test on a C file
cppipe test/c_file.c

# Test the cppipe command
./test/command_line_test.sh

echo ALL TESTS PASSED
