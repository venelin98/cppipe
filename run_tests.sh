#!/bin/sh
set -e

# Test cppipe functions
OKs=$(test/functions_test.cppipe | grep OK | wc -l)
EXPECTED=14
if ! [ $OKs = $EXPECTED ]
then
    echo "Functions test failed: EXPECTED $EXPECTED OKs, got $OKs"
    exit 1
fi
echo Functions test OK!

# Test on a C file
cppipe test/c_file.c

# Test the cppipe command
./test/command_line_test.sh

echo ALL TESTS PASSED
