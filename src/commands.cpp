#include <cstring>
#include <iostream>
#include <assert.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include "commands.hpp"
#include "childProcess.hpp"

enum constants: I32
{
	FILE_PERMISIONS = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
};

static fd_t open_or_die(const char* file, I32 flags)
{
	fd_t fd = open(file, flags, FILE_PERMISIONS);
	if(fd == -1)
	{
		std::cerr << "Can't open: " << file << ' ' << strerror(errno) << std::endl;
		exit(1);
	}
	return fd;
}

RetProc Cmd::operator()(fd_t in, fd_t out, fd_t err)
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

Proc Cmd::detach(fd_t in, fd_t out, fd_t err)
{
	return createProcess(argv.data(), in, out, err);
}

void Cmd::append_args(std::initializer_list<const char*> args)
{
	argv.reserve(argv.size() + args.size());
	argv.back() = *args.begin();
	for(auto it = args.begin() + 1; it < args.end(); ++it)
		argv.push_back(*it);
	argv.push_back(nullptr);
}

Cmd Cmd::operator+(const char* arg)
{
	Cmd result(*this);
	result.argv.back() = arg;
	result.argv.push_back(nullptr);
	return result;
}

Cmd& Cmd::operator+=(const char* arg)
{
	argv.back() = arg;
	argv.push_back(nullptr);
	return *this;
}


PendingCmd::PendingCmd(const Cmd& origin, fd_t in, fd_t out, fd_t err)
	: cmd(origin)
	, in(in)
	, out(out)
	, err(err)
	, execed_(false)
{}

PendingCmd::~PendingCmd()
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

RetProc PendingCmd::operator()()
{
	assert(!execed_ && "Executed command twice");
	execed_ = true;
	return cmd(in, out, err);
}

Proc PendingCmd::detach()
{
	assert(!execed_ && "Executed command twice");
	execed_ = true;
	return cmd.detach(in, out, err);
}

Proc PendingCmd::detachRedirOut()
{
	assert(!execed_ && "Executed command twice"); assert(out==1 && "already redirected");
	assert(out==1 && "capturing redirected proccess");

	execed_ = true;
	return createCapProcess(cmd.argv.data(), in, err);
}

void PendingCmd::cancel()
{
	execed_ = true;
}

std::string $(const PendingCmd& cmd)
{
	Proc p = const_cast<PendingCmd&>(cmd).detachRedirOut();

	// check output size (not portable?)
	// int pipe_size;
	// int rc = ioctl(p.out, FIONREAD, &pipe_size); assert(rc==0);
	// ioctl(p.out, FIONREAD, &pipe_size);

	// write to the string directly, todo: find a better way
	std::string output;

	int read_count;
	int i = 0;
	do
	{
		output.resize(output.size() + PIPE_BUF);   // todo: check if we are overallocating

		read_count = read(p.out, &output[i], PIPE_BUF);  // todo: check errno
		i += read_count;
	}
	while(read_count > 0);
	// p finished?
	output.erase(i, output.size() - i);
	return output;
}

void exec(const Cmd& cmd)
{
	exec_or_die(cmd.argv.data());
}

PendingCmd operator,(const PendingCmd& cfirst, const Cmd& second)
{
	auto& first = const_cast<PendingCmd&>(cfirst);
	first();
	return PendingCmd(second);
}

PendingCmd operator,(RetProc, const Cmd& second)
{
	return PendingCmd(second);
}

PendingCmd operator|(const PendingCmd& cfirst, const Cmd& second)
{
	auto& first = const_cast<PendingCmd&>(cfirst);
	fd_t firstOut = first.detachRedirOut().out;
	return PendingCmd(second, firstOut);
}


RetProc operator&&(const PendingCmd& cfirst, const Cmd& csecond)
{
	auto& first = const_cast<PendingCmd&>(cfirst);
	auto& second = const_cast<Cmd&>(csecond);
	RetProc firstProc = first();

	if(firstProc)
		return second();

	return firstProc;
}

RetProc operator&&(RetProc p, const Cmd& ccmd)
{
	auto& cmd = const_cast<Cmd&>(ccmd);
	if(p)
		return cmd();

	return p;
}

RetProc operator||(const PendingCmd& cfirst, const Cmd& csecond)
{
	auto& first = const_cast<PendingCmd&>(cfirst);
	auto& second = const_cast<Cmd&>(csecond);
	RetProc firstProc = first();

	if(!firstProc)
		return second();

	return firstProc;
}

RetProc operator||(RetProc p, const Cmd& ccmd)
{
	auto& cmd = const_cast<Cmd&>(ccmd);
	if(!p)
		return cmd();

	return p;
}

PendingCmd& operator>(const PendingCmd& cmd, const char* file)
{
	fd_t fd = open_or_die(file, O_WRONLY | O_CREAT);
	return cmd > fd;
}
PendingCmd& operator>(const PendingCmd& ccmd, fd_t fd)
{
	auto& cmd = const_cast<PendingCmd&>(ccmd);
	assert(cmd.out == 1 || !"ERROR: Output is already redirected!");

	cmd.out = fd;
	return cmd;
}

PendingCmd& operator>>(const PendingCmd& cmd, const char* file)
{
	fd_t fd = open_or_die(file, O_WRONLY | O_CREAT | O_APPEND);
	return cmd > fd;
}
PendingCmd& operator>>(const PendingCmd& cmd, fd_t fd)
{
	return cmd > fd;
}

PendingCmd& operator>=(const PendingCmd& cmd, const char* file)
{
	fd_t fd = open_or_die(file, O_WRONLY | O_CREAT);
	return cmd >= fd;
}
PendingCmd& operator>=(const PendingCmd& ccmd, fd_t fd)
{
	auto& cmd = const_cast<PendingCmd&>(ccmd);
	assert(cmd.err == 2 || !"ERROR: Error output is already redirected!");

	cmd.err = fd;
	return cmd;
}

PendingCmd& operator>>=(const PendingCmd& cmd, const char* file)
{
	fd_t fd = open_or_die(file, O_WRONLY | O_CREAT | O_APPEND);
	return cmd >= fd;
}
PendingCmd& operator>>=(const PendingCmd& cmd, fd_t fd)
{
	return cmd >= fd;
}

PendingCmd& operator<(const PendingCmd& cmd, const char* file)
{
	fd_t fd = open_or_die(file, O_RDONLY);
	return cmd < fd;
}
PendingCmd& operator<(const PendingCmd& ccmd, fd_t fd)
{
	auto& cmd = const_cast<PendingCmd&>(ccmd);
	assert(cmd.in == 0 || !"ERROR: Input is already redirected!");

	cmd.in = fd;
	return cmd;
}

PendingCmd operator&(const PendingCmd& cfirst, const Cmd& second)
{
	const_cast<PendingCmd&>(cfirst).detach();
	return PendingCmd(second);
}
