#!/bin/sh
set -e

echo OK | cppipe test/c_file.c

echo ALL TESTS PASSED
