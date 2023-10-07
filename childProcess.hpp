#pragma once

#include "basicTypes.h"

struct Proc
{
	pid_t pid;
	fd_t in;
	fd_t out;
	fd_t err;
};

struct RetProc: public Proc		/* returned process */
{
	RetProc(Proc, int status);	/* status as returned by waitpid */
	bool normal_exit;
	U8 returned;				/* return code */

	/* A returned process evaluates to true if it exited normaly and
	 * returned 0 */
	explicit operator bool();

};

enum Redirect: U32
{
	NOTHING =0,
	INPUT  = 1 << 0,
	OUTPUT = 1 << 1,
	ERR    = 1 << 2				/* todo */
};


/* For all funcions argv is an array of command parameters
 * argv[0] is the command itself, argv needs to end with nullptr */

/* Create a proccess and redirect anyting flaged to a new pipe
   flags - INPUT, OUTPUT, ERR
   return pipe file descriptors*/
Proc createRedirProcess(const char* const argv[], U32 flags);

/* Capture ouput to a pipe, can take alternative input and err FDs */
Proc createCapProcess(const char* const argv[], fd_t in=0, fd_t err=2);

/* Can take FDs as arguments which will be used instead of std */
Proc createProcess(const char* const argv[], fd_t in=0, fd_t out=1, fd_t err=2);

/* Same but waits for the process to finish */
RetProc runProcess(const char* const argv[], fd_t in=0, fd_t out=1, fd_t err=2);

/* execvp the command or exit */
void exec_or_die(const char* const argv[]);
