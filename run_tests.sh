#!/bin/sh
set -e

# Test cppipe functions
OKs=$(cppipe test/functions_test.cppipe | grep OK | wc -l)
if ! [ $OKs = 12 ]
then
    echo "EXPECTED 1 OKs, got $OKs"
    exit 1
fi

# Test on a C file
echo OK | cppipe test/c_file.c

echo ALL TESTS PASSED
