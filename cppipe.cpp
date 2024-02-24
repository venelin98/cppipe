#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <unistd.h>
#include "commands.hpp"

using namespace std;
namespace fs = std::filesystem;

static fs::path find_path_to_cpp(string_view file); // find the cpp to run, if it doesn't exist, exit program
static fs::path get_cache_path(const fs::path& file);	// find the path of the cache for the given file path
static bool compile_file(const fs::path& file, const fs::path& cache, bool debug); // return true on success

int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		cout << "Usage: cppipe [-g] FILE [ARGUMENTS]...\n";
		return 1;
	}
	if( !strcmp(argv[1], "--help") )
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
	if( !strcmp(argv[1], "-g") )
	{
		if(argc < 3)
		{
			cout << "Usage: cppipe [-g] FILE [ARGUMENTS]...\n";
			return 1;
		}

		debug = true;
		file_arg = 2;
	}

	const fs::path file = find_path_to_cpp( argv[file_arg] );

	fs::path cache( get_cache_path(file) );   // cache bins to avoid recompiles

	if(debug)
		cache += "_dbg";

	// Compile the cpp
	if( !compile_file(file, cache, debug) )
		return 1;

	// Run the file without forking
	Cmd run;
	if(debug)			   // debug it with gdb
	{
		run += "gdb";
		run += "--args";
	}

	run += cache.c_str();
	for(int i = file_arg+1; i < argc; ++i)
		run += argv[i];
	exec(run);
}

static fs::path find_path_to_cpp(string_view file)
{
	if(fs::exists(file))	// found relative to CWD
	{
		return file;
	}

	// search files on CPPIPEPATH
	const char* cppipepath_var = getenv("CPPIPEPATH");
	if(cppipepath_var)	// is set
	{
		const string_view cppipepath = cppipepath_var;
		for(size_t begin = 0, end = cppipepath.find(':');
		    ;
		    begin = end+1, end = cppipepath.find(':', begin) )
		{
			const string_view path_entry = cppipepath.substr(begin, end - begin);
			if( fs::is_directory(path_entry) )
			{
				for(const fs::directory_entry& e: fs::directory_iterator(path_entry))
				{
					if(fs::is_regular_file(e) && e.path().filename() == file)
					{
						return e;
					}
				}
			}

			if(end == string::npos)
				break;
		}
	}

	// Couln't find the cpp
	cerr << "File: " << file << " doesn't exist\n";
	exit(1);
}

static fs::path get_cache_path(const fs::path& file)
{
	fs::path cache;
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
	cache += fs::absolute(file);
	fs::create_directories(cache.parent_path());
	return cache;
}

static bool compile_file(const fs::path& file, const fs::path& cache, bool debug)
{
	// Only compile if the source is newer then the bin
	error_code ec;
	if(last_write_time(file) > last_write_time(cache, ec))
	{
		Cmd compile(
			"g++",
			"-o", cache.c_str(),
			"-pipe", "-std=c++17", "-march=native",
			// "-Wall", "-Wextra", "-Wno-parentheses",
			file.c_str(),
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

		if( !compile() )	 // if failed to compile
			return false;
	}
	return true;
}
