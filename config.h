// C compiler
const char* CC  = "cc";

// C++ compiler
const char* CXX = "c++";

// The compilation options are chosen with gcc in mind, but clang should also work
// Other compilers are not tested, but may work
// You can read about gcc options by running "man gcc"

// Compiler options to use...

// ...when debugging
#define DEBUG_FLAGS "-g"

// ...when not debugging
#define RELEASE_FLAGS "-Ofast", "-s"

// ...for both C and C++
#define CPPFLAGS  "-fwhole-program", "-march=native", "-Wall", "-Wextra", "-pipe"

// ...for C
#define CFLAGS CPPFLAGS

// ...for C++
// cppipe command functions use C++17
// Wparentheses is disabled on C++ because of the cppipe functions
#define CXXFLAGS CPPFLAGS, "-std=c++17", "-Wno-parentheses"
