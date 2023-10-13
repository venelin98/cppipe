#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include "childProcess.hpp"


RetProc::RetProc(Proc origin, int status)
	: Proc(origin)
	, normal_exit(WIFEXITED(status))
	, returned(WEXITSTATUS(status))
{}

RetProc::operator bool()
{
	return normal_exit && returned == 0;
}

RetProc wait(Proc p)
{
	int status;
	waitpid(p.pid, &status, 0);
	return RetProc(p, status);
}

/* create a process taking input from pipe childIn */
static Proc createProc(const char* const argv[], const fd_t childIn[2])
{
	Proc childproc;
	childproc.in = childIn[1];
	childproc.out = STDOUT_FILENO;

	childproc.pid = fork();
	if(childproc.pid == 0)	/* child */
	{
		close(childIn[1]);
		dup2(childIn[0], STDIN_FILENO);

		exec_or_die(argv);
	}

	return childproc;
}

/* create a process and redirect it's output to a pipe */
static Proc createRedirProc(const char* const argv[])
{
	fd_t childOut[2];
	pipe(childOut);

	Proc childproc;
	childproc.in = STDIN_FILENO;
	childproc.out = childOut[0];

	childproc.pid = fork();
	/* close(STDIN_FILENO); flush?*/
	if(childproc.pid != 0)				/* parent */
	{
		close(childOut[1]);
	}
	else
	{
		dup2(childOut[1], STDOUT_FILENO);

		exec_or_die(argv);
	}

	return childproc;
}

/* create a process redirecting both in and out */
static Proc createRedirProc(const char* const argv[], const fd_t childIn[2])
{
	fd_t childOut[2];			/* todo: add error #include "processTypes.hpp"*/
	pipe(childOut);

	Proc childproc;
	childproc.in = childIn[1];
	childproc.out = childOut[0];

	childproc.pid = fork();
	/* close(STDIN_FILENO); flush?*/
	if(childproc.pid != 0)				/* parent */
	{
		close(childOut[1]);
	}
	else
	{
		close(childIn[1]);
		dup2(childIn[0], STDIN_FILENO);

		dup2(childOut[1], STDOUT_FILENO);

		exec_or_die(argv);
	}

	return childproc;
}


Proc createRedirProcess(const char* const argv[], U32 flags) /* todo rename */
{
	fd_t childIn[2];
	Proc proc;

	if(flags & INPUT)
	{
		pipe(childIn);
		if(flags & OUTPUT)
			proc = createRedirProc(argv, childIn);
		else
			proc = createProc(argv, childIn);
	}
	else if(flags & OUTPUT)
		proc = createRedirProc(argv);
	else
		proc = createProcess(argv);

	return proc;
}

Proc createCapProcess(const char* const argv[], fd_t in, fd_t err)
{
	fd_t childOut[2];
	pipe(childOut);

	Proc proc;
	proc.in = in;
	proc.out = childOut[0];
	proc.err = err;

	proc.pid = fork();
	if(proc.pid == 0)	/* child */
	{
		if(in != STDIN_FILENO)
			dup2(in, STDIN_FILENO);

		dup2(childOut[1], STDOUT_FILENO);

		if(err != STDERR_FILENO)
			dup2(err, STDERR_FILENO);

		exec_or_die(argv);
	}
	else
	{
		close(childOut[1]);
	}

	return proc;
}

Proc createProcess(const char* const argv[], fd_t in, fd_t out, fd_t err)
{
	Proc proc;
	proc.in = in;
	proc.out = out;
	proc.err = err;

	proc.pid = fork();
	if(proc.pid == 0)	/* child */
	{
		if(in != STDIN_FILENO)
			dup2(in, STDIN_FILENO);

		if(out != STDOUT_FILENO)
			dup2(out, STDOUT_FILENO);

		if(err != STDERR_FILENO)
			dup2(err, STDERR_FILENO);

		exec_or_die(argv);
	}

	return proc;
}

void exec_or_die(const char* const argv[])
{
	execvp(argv[0], (char* const *)argv);

	std::cerr << "Can't execute: " << argv[0] << ' ' << strerror(errno) << std::endl;
	_exit(1);		// _exit since we are a child
}
