#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <fstream>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "commands.hpp"

using namespace std;
namespace fs = std::filesystem;

struct MappedFile		// todo: move to utils
{
	// ~MappedFile()
	// {
	// 	munmap(data, len);
	// }
	char* data;
	unsigned len;
};

// find the cpp to run, if it doesn't exist, exit program
static fs::path find_path_to_cpp(string_view src_file);

// find the path of the cache for the given src_file path
static fs::path get_cache_dir_path(const fs::path& src_file);

// map file in memory with write persmissions
static MappedFile mapfile_for_writing(const fs::path& file);

// preprocess the src file and compare the result to the previous version, return weather it's changed
// remove cached bin if true
static bool preprocess_and_compare();

// only recompile if changes are present
static void compile_src_file();

static const char DEBUG_PREFIX[] = "__DBG";

// Context
static bool debug = false;
static fs::path src_file;
static fs::path cache_dir;
static fs::path bin;   // cache bins to avoid recompiles

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
			"Pass the ARGUMENTS to the compiled binary.\n"
			"The binaries are cached and recompiled only if the source or it's headers have changed.\n"
			"-g debug the binary, asserts are also enabled\n";
		return 0;

	}

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

	// Init context
	src_file = find_path_to_cpp( argv[file_arg] );
	cache_dir = get_cache_dir_path(src_file);
	bin = cache_dir / (debug ? DEBUG_PREFIX : "") += src_file.filename();

	// Compile the src
	compile_src_file();

	// Run the src file without forking
	Cmd run;
	if(debug)			   // debug it with gdb
	{
		run += "gdb";
		run += "--args";
	}

	run += bin.c_str();
	for(int i = file_arg+1; i < argc; ++i)
		run += argv[i];
	exec(run);
}

static fs::path find_path_to_cpp(string_view src_file)
{
	if(fs::exists(src_file))	// found relative to CWD
	{
		return src_file;
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
					if(fs::is_regular_file(e) && e.path().filename() == src_file)
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
	cerr << "File: " << src_file << " doesn't exist\n";
	exit(1);
}

static fs::path get_cache_dir_path(const fs::path& src_file)
{
	fs::path cache_dir;
	if(char* xdg_cache = getenv("XDG_CACHE_HOME"))
	{
		cache_dir = xdg_cache;
		cache_dir /= "cppipe";
	}
	else
	{
		char* home( getenv("HOME") );
		cache_dir = home;
		cache_dir /= ".cache/cppipe";
	}
	cache_dir += fs::canonical( src_file ).parent_path();
	fs::create_directories(cache_dir);
	return cache_dir;
}

static MappedFile mapfile_for_writing(const fs::path& file)
{
	fd_t fd = open(file.c_str(), O_RDWR);

	MappedFile res;
	res.len = lseek(fd, 0, SEEK_END);
	res.data = (char*)mmap(nullptr, res.len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	close(fd);
	return res;
}

static bool preprocess_and_compare()
{
	Cmd preprocess(
		"g++",
		"-E",		// preprocess only
		"-P",		// don't generate linemarkers in the output to reduce file size
		src_file.c_str()
		);
	if(!debug)
		preprocess +=  "-DNDEBUG";

	fs::path old_pp_path = cache_dir / (debug ? DEBUG_PREFIX : "") += src_file.stem() += ".ii";

	string new_pp = $(preprocess);

	if( fs::exists(old_pp_path) ) // todo: clean up if else blocks
	{
		if( fs::file_size(old_pp_path) == new_pp.size() )
		{
			MappedFile old_pp = mapfile_for_writing(old_pp_path);
			if( !memcmp(old_pp.data, &new_pp[0], old_pp.len) ) // unchanged
			{
				return false;
			}
			else
			{
				memcpy(old_pp.data, &new_pp[0], old_pp.len);
				munmap(old_pp.data, old_pp.len);
				return true;
			}
			// todo
			// munmap(data, len);
		}
		else
		{
			fs::remove(bin); // rm old bin
			ofstream pp_file(old_pp_path);
			pp_file << new_pp;
			return true;
		}
	}
	else
	{
		fs::remove(bin); // rm old bin
		ofstream pp_file(old_pp_path);
		pp_file << new_pp;
		return true;
	}
}

static void compile_src_file()
{
	// Only compile if the source is newer then the bin
	if(preprocess_and_compare() || !fs::exists(bin))
	{
		string preprocessed_file = cache_dir / src_file.stem() += ".ii"; // todo: duplicates with old_pp_path // todo: support C
		Cmd compile(
			"g++",
			preprocessed_file.c_str(),
			"-o", bin.c_str(),
			"-pipe", "-std=c++17", "-march=native",
			"-Wall", "-Wextra", "-Wno-parentheses",
			"-lcppipe"
			);

		if(debug)
		{
			compile.append_args({
					"-g"
					// "-Wall", "-Wextra", "-Wno-parentheses"
				});
		}
		else
		{
			compile.append_args({
					"-Ofast", "-flto", "-s"
				});
		}

		if( !compile() )	 // if failed to compile
			exit(1);
	}
}
