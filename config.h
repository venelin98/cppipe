// C compiler
const char* CC  = "cc";

// C++ compiler
const char* CXX = "c++";

// Options are based on gcc, but should work for clang as well
// You can read about gcc options by running "man gcc"

// Compiler options to use...

// ...when debugging
#define DEBUG_FLAGS "-g"

// ...when not debugging
// -O2 seems to work best for both execution and compilation speed
#define RELEASE_FLAGS "-O2", "-s"

// ...for both C and C++
#define CPPFLAGS  "-fwhole-program", "-march=native", "-Wall", "-Wextra", "-pipe"

// ...for C
#define CFLAGS CPPFLAGS

// ...for C++
// cppipe command functions use C++17
// Wparentheses is disabled on C++ because of the cppipe functions
#define CXXFLAGS CPPFLAGS, "-std=c++17", "-Wno-parentheses"
