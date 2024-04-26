// C compiler
const char* CC  = "gcc";

// C++ compiler
const char* CXX = "g++";

// You can read about gcc options by running "man gcc"
// Compiler options to use...

// ...when debugging
#define DEBUG_FLAGS "-g"

// ...when not debugging
#define RELEASE_FLAGS "-Ofast", "-flto", "-s"

// ...for both C and C++
#define CPPFLAGS "-pipe", "-march=native", "-Wall", "-Wextra"

// ...for C
#define CFLAGS CPPFLAGS

// ...for C++
// cppipe command functions use C++17
// Wparentheses is disabled on C++ because of the cppipe functions
#define CXXFLAGS CPPFLAGS, "-std=c++17", "-Wno-parentheses", "-lcppipe"
