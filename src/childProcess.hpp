#pragma once

#include "basicTypes.h"
#include <optional>

struct Proc
{
	pid_t pid;
	fd_t in;
	fd_t out;
	fd_t err;
};

/* process that has finished for any reason */
struct DeadProc: public Proc
{
	DeadProc(Proc, int status);	/* status as returned by waitpid */
	bool normal_exit;		/* todo: std::optional? */
	U8 exit_status;				/* only use if normal_exit */

	/* A returned process evaluates to true if it exited normaly and
	 * returned 0 */
	explicit operator bool();
};

/* Wait for a running proccess to finish */
DeadProc wait(Proc);

/* Check if the Proc has exited and return it's DeadProc if it has */
std::optional<DeadProc> check_exited(Proc);

enum { PIPE = -1 };

/* Create a proccess
 * argv is an array of command parameters
 * argv[0] is the command itself, argv needs to end with nullptr
 * we don't wait for the proccess to finish
 *
 * Can take FDs as arguments which will be used instead of std
 * If PIPE is given for any FD, a pipe is created and input/output/error is redirected there
 * The pipe can then be read (out, err) or written to (in)
 */
Proc createProcess(const char* const argv[], fd_t in=0, fd_t out=1, fd_t err=2);

/* execvp the command or exit */
void exec_or_die(const char* const argv[]);

#include "childProcess.inl"
