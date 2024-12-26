#include <cstring>
#include <iostream>
#include <assert.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "commands.hpp"
#include "childProcess.hpp"

enum constants: I32
{
	FILE_PERMISIONS = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
};

namespace _cppipe
{
	inline fd_t open_or_die(const char* file, I32 flags)
	{
		fd_t fd = open(file, flags, FILE_PERMISIONS);
		if(fd == -1)
		{
			std::cerr << "Can't open: " << file << ' ' << strerror(errno) << std::endl;
			exit(1);
		}
		return fd;
	}
}

inline DeadProc Cmd::operator()(fd_t in, fd_t out, fd_t err) const
{
	Proc p = createProcess(argv.data(), in, out, err);
	return wait(p);
}

inline void Cmd::append_args(std::initializer_list<const char*> args)
{
	argv.reserve(argv.size() + args.size());
	argv.back() = *args.begin();
	for(auto it = args.begin() + 1; it < args.end(); ++it)
		argv.push_back(*it);
	argv.push_back(nullptr);
}

inline Cmd Cmd::operator+(const char* arg)
{
	Cmd result(*this);
	result.argv.back() = arg;
	result.argv.push_back(nullptr);
	return result;
}

inline Cmd& Cmd::operator+=(const char* arg)
{
	argv.back() = arg;
	argv.push_back(nullptr);
	return *this;
}


inline PendingCmd::PendingCmd(const Cmd& origin, fd_t in, fd_t out, fd_t err)
	: cmd(origin)
	, in(in)
	, out(out)
	, err(err)
	, execed_(false)
{}

inline PendingCmd::~PendingCmd()
{
	if(!execed_)
		cmd(in, out, err);
}

inline DeadProc PendingCmd::operator()()
{
	assert(!execed_ && "Executed command twice");
	execed_ = true;
	return cmd(in, out, err);
}

inline void PendingCmd::cancel()
{
	execed_ = true;
}

inline std::string $(const PendingCmd& c)
{
	Proc p = detachRedirOut(c);

	std::string output = read_to_end(p.out);

	// Remove trailing newlines
	int i = output.size() - 1;
	while(i > 0 && output[i] == '\n')
		--i;

	// erase the newlines
	output.erase(i+1, -1); 	// till the end

	return output;
}

std::string read_to_end(fd_t fd)
{
	// write to the string directly, todo: find a better way
	std::string output;

	int read_count;
	int i = 0;
	do
	{
		output.resize(output.size() + PIPE_BUF);   // todo: check if we are overallocating

		read_count = read(fd, &output[i], PIPE_BUF);  // todo: check errno
		if(read_count > 0)
			i += read_count;
	}
	while(read_count > 0);

	close(fd);

	// erase the extra elements
	output.erase(i, -1); 	// till the end

	return output;
}

inline void exec(const Cmd& c)
{
	exec_or_die(c.argv.data());
}

inline DeadProc run(const PendingCmd& c)
{
	return const_cast<PendingCmd&>(c)();
}

inline Proc detach(const PendingCmd& ccmd)
{
	auto& c = const_cast<PendingCmd&>(ccmd);
	assert(!c.execed_ && "Executed command twice");

	c.execed_ = true;
	return createProcess(c.cmd.argv.data(), c.in, c.out, c.err);
}

inline Proc detachRedirIn(const PendingCmd& ccmd)
{
	auto& c = const_cast<PendingCmd&>(ccmd);
	assert(c.in==0 && "Capturing redirected output");

	c.in = PIPE;

	return detach(c);
}

inline Proc detachRedirOut(const PendingCmd& ccmd)
{
	auto& c = const_cast<PendingCmd&>(ccmd);
	assert(c.out==1 && "Capturing redirected output");

	c.out = PIPE;

	return detach(c);
}

inline Proc detachRedirInOut(const PendingCmd& ccmd)
{
	auto& c = const_cast<PendingCmd&>(ccmd);
	assert(c.in==0 && "Rediredcting redirected input");

	c.in = PIPE;

	return detachRedirOut(c);
}

inline PendingCmd operator,(const PendingCmd& left, const Cmd& right)
{
	run(left);
	return PendingCmd(right);
}

inline PendingCmd operator,(DeadProc, const Cmd& right)
{
	return PendingCmd(right);
}

inline PendingCmd operator|(const PendingCmd& left, const Cmd& right)
{
	fd_t leftOut = detachRedirOut(left).out;
	return PendingCmd(right, leftOut);
}


inline DeadProc operator&&(const PendingCmd& cleft, const Cmd& cright)
{
	auto& left = const_cast<PendingCmd&>(cleft);
	auto& right = const_cast<Cmd&>(cright);
	DeadProc leftProc = left();

	if(leftProc)
		return right();

	return leftProc;
}

inline DeadProc operator&&(DeadProc p, const Cmd& ccmd)
{
	auto& cmd = const_cast<Cmd&>(ccmd);
	if(p)
		return cmd();

	return p;
}

inline DeadProc operator||(const PendingCmd& cleft, const Cmd& cright)
{
	auto& left = const_cast<PendingCmd&>(cleft);
	auto& right = const_cast<Cmd&>(cright);
	DeadProc leftProc = left();

	if(!leftProc)
		return right();

	return leftProc;
}

inline DeadProc operator||(DeadProc p, const Cmd& ccmd)
{
	auto& cmd = const_cast<Cmd&>(ccmd);
	if(!p)
		return cmd();

	return p;
}

inline PendingCmd& operator>(const PendingCmd& c, const char* file)
{
	fd_t fd = _cppipe::open_or_die(file, O_WRONLY | O_CREAT);
	return c > fd;
}
inline PendingCmd& operator>(const PendingCmd& ccmd, fd_t fd)
{
	auto& c = const_cast<PendingCmd&>(ccmd);
	assert(c.out == 1 && "ERROR: Output is already redirected!");

	c.out = fd;
	return c;
}

inline PendingCmd& operator>>(const PendingCmd& c, const char* file)
{
	fd_t fd = _cppipe::open_or_die(file, O_WRONLY | O_CREAT | O_APPEND);
	return c > fd;
}
inline PendingCmd& operator>>(const PendingCmd& c, fd_t fd)
{
	return c > fd;
}

inline PendingCmd& operator>=(const PendingCmd& c, const char* file)
{
	fd_t fd = _cppipe::open_or_die(file, O_WRONLY | O_CREAT);
	return c >= fd;
}
inline PendingCmd& operator>=(const PendingCmd& ccmd, fd_t fd)
{
	auto& c = const_cast<PendingCmd&>(ccmd);
	assert(c.err == 2 && "ERROR: Error output is already redirected!");

	c.err = fd;
	return c;
}

inline PendingCmd& operator>>=(const PendingCmd& c, const char* file)
{
	fd_t fd = _cppipe::open_or_die(file, O_WRONLY | O_CREAT | O_APPEND);
	return c >= fd;
}
inline PendingCmd& operator>>=(const PendingCmd& c, fd_t fd)
{
	return c >= fd;
}

inline PendingCmd& operator<(const PendingCmd& c, const char* file)
{
	fd_t fd = _cppipe::open_or_die(file, O_RDONLY);
	return c < fd;
}
inline PendingCmd& operator<(const PendingCmd& ccmd, fd_t fd)
{
	auto& c = const_cast<PendingCmd&>(ccmd);
	assert(c.in == 0 && "ERROR: Input is already redirected!");

	c.in = fd;
	return c;
}

inline PendingCmd operator&(const PendingCmd& left, const Cmd& right)
{
	detach(left);
	return PendingCmd(right);
}

inline std::ostream& operator<<(std::ostream& s, const Cmd& c)
{
	for(size_t i = 0; i < c.argv.size() - 1; ++i)
		s << '"' << c.argv[i] << '"' << ' ';
	return s;
}
