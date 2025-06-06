#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string_view>
#include <optional>

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

// find the source file to run, if it doesn't exist, exit program
fs::path find_path_to_src(string_view src_file);

// find the path of the cache for the given src_file path
fs::path get_cache_dir_path(const fs::path& src_file);

// map file in memory with write persmissions
MappedFile mapfile_for_writing(const fs::path& file);

// preprocess the src file, compare and overwrite the result to the previous version
// return the path of the preprocessed file if it's different
optional<fs::path> preprocess_and_compare();

// only recompile if changes are present
void compile_src_file();

void print_usage();

SrcType find_src_type(string_view path);

const char DEBUG_PREFIX[] = "__DBG";

// Context
fs::path HOME;
fs::path src_file;
SrcType src_type;
fs::path cache_dir;
fs::path bin;   // cache bins to avoid recompiles

// Options
bool debug = false;
// just compare timestamps of the source and bin, dont preprocess
bool quick = false;

}

int main(int argc, char* argv[])
{
	int src_arg = parse_args_until_src(argc, argv);

	// Init context
	HOME = getenv("HOME");
	src_file = find_path_to_src( argv[src_arg] );
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
			cout << "Compile and run C/C++ source FILE\n"
				"Pass the ARGUMENTS to the compiled binary\n"
				"The binaries are cached and recompiled only if the source or it's headers have changed\n\n"
				"Options:\n"
				"-g debug the binary, asserts are also enabled\n"
				"-q quicker, just compare source and binary time stamps, if included files were updated a recompile WON'T occur!\n\n"
				"Environment variables:\n"
				"CPPIPEPATH - ':'-separated list of directories to prepend to the FILE search path\n";
			exit(0);
		}
		else if( arg == "-g" )
		{
			debug = true;
		}
		else if( arg == "-q" )
		{
			quick = true;
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

fs::path find_path_to_src(string_view src_file)
{
	if(fs::exists(src_file))	// Found relative to CWD
	{
		return src_file;
	}

	// Search files on CPPIPEPATH
	const char* cppipepath_var = getenv("CPPIPEPATH");
	if(cppipepath_var)	// is set
	{
		const string_view cppipepath = cppipepath_var;
		for(size_t begin = 0, end = cppipepath.find(':');
		    end != string::npos;
		    begin = end+1, end = cppipepath.find(':', begin) )
		{
			const fs::path path_entry = cppipepath.substr(begin, end - begin);

			if( fs::path p = path_entry / src_file; fs::exists(p) )
			{
				return p;
			}
		}
	}

	// Couln't find the src
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
		cache_dir = HOME / ".cache/cppipe";
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

optional<fs::path> preprocess_and_compare()
{
	Cmd preprocess(
		src_type == SrcType::C ? CC : CXX,
		"-E"		// preprocess only
		#ifdef __OpenBSD__
		,"-I/usr/local/include" // not included by default on OpenBSD
		#endif
		);

	// Add preprocessor flags
	if(src_type == SrcType::C)
	{
		preprocess.append_args({ CFLAGS });
	}
	else
	{
		preprocess.append_args({ CXXFLAGS });
		preprocess += "-xc++"; // treat the file as a .cpp
	}

	// Read source from stdin
	preprocess += "-";


	if(!debug)
		preprocess +=  "-DNDEBUG";

	// Preprocessed file from the previous run
	fs::path old_pp_path = cache_dir / (debug ? DEBUG_PREFIX : "") += src_file.stem()
		+= (src_type == SrcType::C ? ".i" : ".ii");

	Proc preprocessing = detachRedirInOut(preprocess);

	// File to preprocess
	MappedFile src = mapfile_for_writing(src_file);

	// If it begins with #! skip the first line
	const char* p = src.data;
	if(src.len > 1 && p[0] == '#' && p[1] == '!')
		while(*p != '\n')
			++p;

	write(preprocessing.in, p, src.len - (p - src.data));
	close(preprocessing.in);

	// Result of preprocessing as string
	string new_pp = read_to_end(preprocessing.out);

	if( !wait(preprocessing) )	// preprocessing failed
		exit(1);

	if( fs::exists(old_pp_path) ) // todo: clean up if else blocks
	{
		if( fs::file_size(old_pp_path) == new_pp.size() )
		{
			MappedFile old_pp = mapfile_for_writing(old_pp_path);
			if( !memcmp(old_pp.data, &new_pp[0], old_pp.len) ) // unchanged
			{
				return nullopt;
			}
			else
			{
				memcpy(old_pp.data, &new_pp[0], old_pp.len);
				munmap(old_pp.data, old_pp.len);
				return old_pp_path;
			}
			// todo
			// munmap(data, len);
		}
		else
		{
			ofstream pp_file(old_pp_path);
			pp_file << new_pp;
			return old_pp_path;
		}
	}
	else
	{
		ofstream pp_file(old_pp_path);
		pp_file << new_pp;
		return old_pp_path;
	}
}

void compile_src_file()
{
	if(quick)
	{
		error_code ec;
		if(fs::last_write_time(src_file) < fs::last_write_time(bin, ec))
		{
			// Binary is newer then source, dont recompile
			return;
		}
	}

	optional<fs::path> new_preprocessed = preprocess_and_compare();

	// Only compile if the source is newer then the bin
	if(new_preprocessed || !fs::exists(bin))
	{

		Cmd compile(
			src_type == SrcType::C ? CC : CXX,
			new_preprocessed->c_str(),
			"-o", bin.c_str()
			);


		if(src_type == SrcType::C)
			compile.append_args({ CFLAGS });
		else
			compile.append_args({ CXXFLAGS });


		if(debug)
			compile.append_args({ DEBUG_FLAGS });
		else
			compile.append_args({ RELEASE_FLAGS });


		if( !compile() )	 // if failed to compile
			exit(1);
	}
}

void print_usage()
{
	cout << "Usage: cppipe [OPTION]... FILE [ARGUMENTS]...\n";
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
