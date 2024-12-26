#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include "childProcess.hpp"


inline DeadProc::DeadProc(Proc origin, int status)
	: Proc(origin)
	, normal_exit(WIFEXITED(status))
	, exit_status(WEXITSTATUS(status))
{}

inline DeadProc::operator bool()
{
	return normal_exit && exit_status == 0;
}

inline DeadProc wait(Proc p)
{
	int status;
	if( waitpid(p.pid, &status, 0) == -1 )
	{
		std::cerr << "waitpid encountered an error: " << strerror(errno) << std::endl;
		exit(1);
	}
	return DeadProc(p, status);
}

inline std::optional<DeadProc> check_exited(Proc p)
{
	std::optional<DeadProc> result;

	int status;
	pid_t rc = waitpid(p.pid, &status, WNOHANG);
	if(rc == 0)		// still running
	{
		result = std::nullopt;
	}
	else if(rc == -1)	// waitpid error
	{
		std::cerr << "waitpid encountered an error: " << strerror(errno) << std::endl;
		exit(1);
	}
	else			// finished
	{
		result = DeadProc(p, status);
	}

	return result;
}

namespace _cppipe
{
	/* Create a proccess */
	inline Proc create(const char* const argv[], fd_t in, fd_t out, fd_t err)
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

	/* Create a process taking input from pipe childIn */
	inline Proc createInRedirected(const char* const argv[], const fd_t childIn[2])
	{
		Proc childproc;
		childproc.in = childIn[1];
		childproc.out = STDOUT_FILENO;
		childproc.err = STDERR_FILENO;

		childproc.pid = fork();
		if(childproc.pid == 0)	/* child */
		{
			close(childIn[1]);
			dup2(childIn[0], STDIN_FILENO);

			exec_or_die(argv);
		}

		return childproc;
	}

	/* Like createInRedirected but redirect output to a new pipe */
	inline Proc createInOutRedirected(const char* const argv[], const fd_t childIn[2])
	{
		fd_t childOut[2];			/* todo: add error #include "processTypes.hpp"*/
		pipe(childOut);

		Proc childproc;
		childproc.in = childIn[1];
		childproc.out = childOut[0];
		childproc.err = STDERR_FILENO;

		childproc.pid = fork();
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

	inline Proc createCapProcess(const char* const argv[], fd_t in, fd_t err)
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
		else			/* parent */
		{
			close(childOut[1]); /* close write side */
		}

		return proc;
	}
}

inline Proc createProcess(const char* const argv[], fd_t in, fd_t out, fd_t err)
{
	Proc proc;

	if(in == PIPE)
	{
		fd_t childIn[2];
		pipe(childIn);

		if(out == PIPE)
			proc = _cppipe::createInOutRedirected(argv, childIn);
		else
			proc = _cppipe::createInRedirected(argv, childIn);

		close(childIn[0]); // parent closes read side
	}
	else if(out == PIPE)
		proc = _cppipe::createCapProcess(argv, in, err);
	else			/* todo: err == PIPE */
		proc = _cppipe::create(argv, in, out, err);

	return proc;
}

inline void exec_or_die(const char* const argv[])
{
	execvp(argv[0], (char* const *)argv);

	std::cerr << "Can't execute: " << argv[0] << ' ' << strerror(errno) << std::endl;
	_exit(1);		// _exit since we are a child
}
