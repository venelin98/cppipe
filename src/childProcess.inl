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
	inline void redirect(fd_t new_fd, fd_t old_fd)
	{
		if(new_fd != old_fd)
			dup2(new_fd, old_fd);
	}
}

inline Proc createProcess(const char* const argv[], fd_t in, fd_t out, fd_t err)
{
	using _cppipe::redirect;

	bool in_redir = in == PIPE;
	bool out_redir = out == PIPE;
	bool err_redir = err == PIPE;

	Proc p;

	fd_t childIn[2], childOut[2], childErr[2];
	if(in_redir)
	{
		pipe(childIn);
		p.in  = childIn[1];
	}
	else
		p.in = in;

	if(out_redir)
	{
		pipe(childOut);
		p.out = childOut[0];
	}
	else
		p.out = out;

	if(err_redir)
	{
		pipe(childErr);
		p.err = childErr[0];
	}
	else
		p.err = err;

	p.pid = fork();
	if(p.pid == 0)	/* child */
	{
		/* Close write side */
		if(in_redir)
			close(childIn[1]);

		/* Close read side */
		if(out_redir)
			close(childOut[0]);
		if(err_redir)
			close(childErr[0]);

		/* Take pipe as standart in, out, err */
		redirect(in_redir ? childIn[0] : in, STDIN_FILENO);
		redirect(out_redir ? childOut[1] : out, STDOUT_FILENO);
		redirect(err_redir ? childErr[1] : err, STDERR_FILENO);

		exec_or_die(argv);
	}
	else			/* parent */
	{
		/* Close read side */
		if(in_redir)
			close(childIn[0]);

		/* Close write side */
		if(out_redir)
			close(childOut[1]);
		if(err_redir)
			close(childErr[1]);
	}

	return p;
}

inline void exec_or_die(const char* const argv[])
{
	execvp(argv[0], (char* const *)argv);

	std::cerr << "Can't execute command: " << argv[0] << ' ' << strerror(errno) << '\n';

	/* for(const char* const * arg = argv; *arg != nullptr; ++arg) */
		/* std::cerr << *arg << '\n'; */
	_exit(1);		// _exit since we are a child
}
