#!/bin/sh
set -e

export CPPIPEPATH="/:test::/usr"

cppipe c_file.c > /dev/null

echo Command line test: OK!
