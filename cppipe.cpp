#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <unistd.h>
#include "commands.hpp"

using namespace std;
using namespace std::filesystem;

static path get_cache_path(const char* file);
static bool compile_file(const char* file, path cache, bool debug); // return true on success

int main(int argc, char* argv[])
{
     if(argc < 2)
     {
	  cout << "Usage: cppipe [-g] FILE [ARGUMENTS]...\n";
	  return 1;
     }
     if(!strcmp(argv[1], "--help"))
     {
	  cout << "Usage: cppipe [-g] FILE [ARGUMENTS]...\n"
	       << "Compile and run C++ source FILE that uses the cppipe library.\n"
	          "Pass the ARGUMENTS to he compiled binary.\n"
	          "The binaries are cached and recompiled only if the source is newer.\n"
	          "-g debug the binary, asserts are also enabled\n";
	  return 0;

     }

     bool debug = false;
     int file_arg = 1;
     if(!strcmp(argv[1], "-g"))
     {
	  if(argc < 3)
	  {
	       cout << "Usage: cppipe [-g] FILE [ARGUMENTS]...\n";
	       return 1;
	  }

	  debug = true;
	  file_arg = 2;
     }

     const char* file = argv[file_arg];
     if( !exists(file) )
     {
	  cerr << "File: " << file << " doesn't exist\n";
	  exit(1);
     }

     path cache( get_cache_path(file) );   // cache bins to avoid recompiles

     if(debug)
	  cache += "_dbg";

     // Compile the passed file
     if( !compile_file(file, cache, debug) )
	  return 1;

     // Run the file without forking
     Cmd run;
     if(debug)			// debug it with gdb
     {
	  run += "gdb";
	  run += "--args";
     }

     run += cache.c_str();
     for(int i = file_arg; i < argc; ++i)
	  run += argv[i];
     exec(run);
}

static path get_cache_path(const char* file)
{
     path cache;
     if(char* xdg_cache = getenv("XDG_CACHE_HOME"))
     {
	  cache = xdg_cache;
	  cache /= "cppipe";
     }
     else
     {
	  char* home( getenv("HOME") );
	  cache = home;
	  cache /= ".cache/cppipe";
     }
     cache += absolute(file);
     create_directories(cache.parent_path());
     return cache;
}

static bool compile_file(const char* file, path cache, bool debug)
{
     // Only compile if the source is newer then the bin or we are in debug
     error_code ec;
     if(last_write_time(file) > last_write_time(cache, ec))
     {
	  Cmd compile(
	       "g++",
	       "-o", cache.c_str(),
	       "-pipe", "-std=c++17", "-march=native",
	       // "-Wall", "-Wextra", "-Wno-parentheses",
	       file,
	       "-lcppipe"
	       );

	  if(debug)
	  {
	       compile.append_args({
			 "-g",
			 "-Wall", "-Wextra", "-Wno-parentheses"
		    });
	  }
	  else
	  {
	       compile.append_args({
			 "-Ofast", "-flto", "-DNDEBUG", "-s"
		    });
	  }

	  if( !compile() )	// if failed to compile
	       return false;
     }
     return true;
}
