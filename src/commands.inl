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

	// Close files if such were used
	// if(in > 2)
	// 	close(in);
	// if(out > 2)
	// 	close(out);
	// if(err > 2)
	// 	close(err);

	return wait(p);
}

inline Proc Cmd::detach(fd_t in, fd_t out, fd_t err) const
{
	return createProcess(argv.data(), in, out, err);
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
		operator()();
	/* if(in != 0) */
	/* 	close(in); */
	/* if(out != 1) */
	/* 	close(out); */
	/* if(err != 2) */
	/* 	close(err); */
}

inline DeadProc PendingCmd::operator()()
{
	assert(!execed_ && "Executed command twice");
	execed_ = true;
	return cmd(in, out, err);
}

inline Proc PendingCmd::detach()
{
	assert(!execed_ && "Executed command twice");
	execed_ = true;
	return cmd.detach(in, out, err);
}

inline Proc PendingCmd::detachRedirOut()
{
	assert(!execed_ && "Executed command twice");
	assert(out==1 && "Capturing redirected proccess");

	execed_ = true;
	return createCapProcess(cmd.argv.data(), in, err);
}

inline void PendingCmd::cancel()
{
	execed_ = true;
}

inline std::string $(const PendingCmd& cmd)
{
	Proc p = const_cast<PendingCmd&>(cmd).detachRedirOut();

	// write to the string directly, todo: find a better way
	std::string output;

	int read_count;
	int i = 0;
	do
	{
		output.resize(output.size() + PIPE_BUF);   // todo: check if we are overallocating

		read_count = read(p.out, &output[i], PIPE_BUF);  // todo: check errno
		if(read_count > 0)
			i += read_count;
	}
	while(read_count > 0);
	// p finished?

	// erase the extra elements
	output.erase(i, output.size() - i);

	close(p.out);

	return output;
}

inline void exec(const Cmd& cmd)
{
	exec_or_die(cmd.argv.data());
}

DeadProc run(const Cmd& cmd)
{
	return cmd();
}

inline PendingCmd operator,(const PendingCmd& cleft, const Cmd& right)
{
	auto& left = const_cast<PendingCmd&>(cleft);
	left();
	return PendingCmd(right);
}

inline PendingCmd operator,(DeadProc, const Cmd& right)
{
	return PendingCmd(right);
}

inline PendingCmd operator|(const PendingCmd& cleft, const Cmd& right)
{
	auto& left = const_cast<PendingCmd&>(cleft);
	fd_t leftOut = left.detachRedirOut().out;
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

inline PendingCmd& operator>(const PendingCmd& cmd, const char* file)
{
	fd_t fd = _cppipe::open_or_die(file, O_WRONLY | O_CREAT);
	return cmd > fd;
}
inline PendingCmd& operator>(const PendingCmd& ccmd, fd_t fd)
{
	auto& cmd = const_cast<PendingCmd&>(ccmd);
	assert(cmd.out == 1 && "ERROR: Output is already redirected!");

	cmd.out = fd;
	return cmd;
}

inline PendingCmd& operator>>(const PendingCmd& cmd, const char* file)
{
	fd_t fd = _cppipe::open_or_die(file, O_WRONLY | O_CREAT | O_APPEND);
	return cmd > fd;
}
inline PendingCmd& operator>>(const PendingCmd& cmd, fd_t fd)
{
	return cmd > fd;
}

inline PendingCmd& operator>=(const PendingCmd& cmd, const char* file)
{
	fd_t fd = _cppipe::open_or_die(file, O_WRONLY | O_CREAT);
	return cmd >= fd;
}
inline PendingCmd& operator>=(const PendingCmd& ccmd, fd_t fd)
{
	auto& cmd = const_cast<PendingCmd&>(ccmd);
	assert(cmd.err == 2 && "ERROR: Error output is already redirected!");

	cmd.err = fd;
	return cmd;
}

inline PendingCmd& operator>>=(const PendingCmd& cmd, const char* file)
{
	fd_t fd = _cppipe::open_or_die(file, O_WRONLY | O_CREAT | O_APPEND);
	return cmd >= fd;
}
inline PendingCmd& operator>>=(const PendingCmd& cmd, fd_t fd)
{
	return cmd >= fd;
}

inline PendingCmd& operator<(const PendingCmd& cmd, const char* file)
{
	fd_t fd = _cppipe::open_or_die(file, O_RDONLY);
	return cmd < fd;
}
inline PendingCmd& operator<(const PendingCmd& ccmd, fd_t fd)
{
	auto& cmd = const_cast<PendingCmd&>(ccmd);
	assert(cmd.in == 0 && "ERROR: Input is already redirected!");

	cmd.in = fd;
	return cmd;
}

inline PendingCmd operator&(const PendingCmd& cleft, const Cmd& right)
{
	const_cast<PendingCmd&>(cleft).detach();
	return PendingCmd(right);
}

inline std::ostream& operator<<(std::ostream& s, const Cmd& cmd)
{
	for(size_t i = 0; i < cmd.argv.size() - 1; ++i)
		s << '"' << cmd.argv[i] << '"' << ' ';
	return s;
}
