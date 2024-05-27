#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string_view>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "commands.hpp"
#include "../config.h"

using namespace std;
namespace fs = std::filesystem;

enum SrcType
{
	C,
	CPP
};

namespace
{

struct MappedFile		// todo: move to utils
{
	// ~MappedFile()
	// {
	// 	munmap(data, len);
	// }
	char* data;
	unsigned len;
};

// process the args until the src file arg is found, return its index
int parse_args_until_src(int argc, char* argv[]);

// find the cpp to run, if it doesn't exist, exit program
fs::path find_path_to_cpp(string_view src_file);

// find the path of the cache for the given src_file path
fs::path get_cache_dir_path(const fs::path& src_file);

// map file in memory with write persmissions
MappedFile mapfile_for_writing(const fs::path& file);

// preprocess the src file and compare the result to the previous version, return weather it's changed
// remove cached bin if true
bool preprocess_and_compare();

// only recompile if changes are present
void compile_src_file();

void print_usage();

SrcType find_src_type(string_view path);

const char DEBUG_PREFIX[] = "__DBG";

// Context
bool debug = false;
fs::path src_file;
SrcType src_type;
fs::path cache_dir;
fs::path bin;   // cache bins to avoid recompiles

}

int main(int argc, char* argv[])
{
	int src_arg = parse_args_until_src(argc, argv);

	// Init context
	src_file = find_path_to_cpp( argv[src_arg] );
	src_type = find_src_type( argv[src_arg] );
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
	for(int i = src_arg+1; i < argc; ++i)
		run += argv[i];
	exec(run);
}

namespace
{

int parse_args_until_src(int argc, char* argv[])
{
	if(argc < 2)
	{
		print_usage();
		exit(1);
	}

	int src_arg = 0;
	for(int i = 1; i < argc; ++i)
	{
		string_view arg( argv[i] );
		if( arg ==  "--help" )
		{
			print_usage();
			cout << "Compile and run C++ source CPP_FILE that uses the cppipe library.\n"
				"Pass the ARGUMENTS to the compiled binary.\n"
				"The binaries are cached and recompiled only if the source or it's headers have changed.\n"
				"-g debug the binary, asserts are also enabled\n";
			exit(0);
		}
		else if( arg == "-g" )
		{
			debug = true;
		}
		else		// Then treat it as the src_arg
		{
			src_arg = i;
			break;
		}

	}

	if( !src_arg )
	{
		print_usage();
		exit(1);
	}

	return src_arg;
}

fs::path find_path_to_cpp(string_view src_file)
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

fs::path get_cache_dir_path(const fs::path& src_file)
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

MappedFile mapfile_for_writing(const fs::path& file)
{
	fd_t fd = open(file.c_str(), O_RDWR);

	MappedFile res;
	res.len = lseek(fd, 0, SEEK_END);
	res.data = (char*)mmap(nullptr, res.len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	close(fd);
	return res;
}

bool preprocess_and_compare()
{
	Cmd preprocess(
		src_type == SrcType::C ? CC : CXX,
		"-E"		// preprocess only
		);

	if(src_type == SrcType::C)
	{
		for(const char* flag: { CFLAGS })
			preprocess += flag;
	}
	else
	{
		for(const char* flag: { CXXFLAGS })
			preprocess += flag;
		preprocess += "-xc++"; // treat the file as a cpp
	}

	preprocess += src_file.c_str();


	if(!debug)
		preprocess +=  "-DNDEBUG";

	fs::path old_pp_path = cache_dir / (debug ? DEBUG_PREFIX : "") += src_file.stem()
		+= (src_type == SrcType::C ? ".i" : ".ii");

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

void compile_src_file()
{
	// Only compile if the source is newer then the bin
	if(preprocess_and_compare() || !fs::exists(bin))
	{
		string preprocessed_file = cache_dir / src_file.stem() +=
			(src_type == SrcType::C ? ".i" : ".ii"); // todo: duplicates with old_pp_path
		Cmd compile(
			src_type == SrcType::C ? CC : CXX,
			preprocessed_file.c_str(),
			"-o", bin.c_str()
			);

		if(src_type == SrcType::C)
		{
			for(const char* flag: { CFLAGS })
				compile += flag;
		}
		else
		{
			for(const char* flag: { CXXFLAGS })
				compile += flag;
		}



		if(debug)
		{
			compile.append_args({ DEBUG_FLAGS });
		}
		else
		{
			compile.append_args({ RELEASE_FLAGS });
		}

		if( !compile() )	 // if failed to compile
			exit(1);
	}
}

void print_usage()
{
	cout << "Usage: cppipe [-g] CPP_FILE [ARGUMENTS]...\n";
}

SrcType find_src_type(const string_view p)
{
	size_t end = p.size() - 1;

	// if(p.size() > 4 && p[end-3] == '.' && p[end-2] == 'c' && p[end-1] == 'p' && p[end] == 'p')
	// {
	// 	src_type = SrcType::CPP
	// }

	if( p.size() > 2 && p[end-1] == '.' && p[end] == 'c' )
	{
		return SrcType::C;
	}
	else			// treat it as cpp
	{
		return SrcType::CPP;
	}
}

}
