#pragma once

#include <vector>
#include <string>
#include "childProcess.hpp"

/* All CONST references are used and cast away to allow for taking
   both l and r values without using templates or making copies

   char* arg pointers are kept and used NOT COPIED

   Anything that takes a PendingCmd takes a Cmd as well
*/

/* A shell command */
class Cmd
{
public:
	/* all args must be const char* */
	template<typename... Args>
	explicit Cmd(Args... args)
		: argv({args...})
	{
		argv.push_back(nullptr);
	}

	/* Execute the command, if arguments are not given use stdin,out,err
	   else use the given file desciptors. this can also be used
	   to redirect err to out by giving err=1 like in shell */
	DeadProc operator()(fd_t in=0, fd_t out=1, fd_t err=2) const;

	/* Append arguments */
	void append_args(std::initializer_list<const char*>);
	/* Append an argument and return the new command */
	Cmd operator+(const char* arg);
	/* Append an argument */
	Cmd& operator+=(const char* arg);

	std::vector<const char*> argv; /* null terminateded arg list */
};

/*  An instance of a shell comand that is pending execution
    if the command is not executed during the life of the obj
    its executed on destruction */
class PendingCmd
{
public:
	/* Can be implicitly created from a command */
	PendingCmd(const Cmd&, fd_t in=0, fd_t out=1, fd_t err=2);
	/* Execute the command on destruction */
	~PendingCmd();

	/* No copy - we use unnamed return value optimization to return PendingCmd without destruction */
	PendingCmd(const PendingCmd&) = delete;
	PendingCmd& operator=(const PendingCmd&) = delete;

	/* Run the command */
	DeadProc operator()();

	/* Prevent a pending command from being executed on destruction */
	void cancel();

	const Cmd& cmd;
	fd_t in, out, err;
private:
	bool execed_;

	friend Proc detach(const PendingCmd&);
	friend Proc detachRedirOut(const PendingCmd&);
};


/* Like shell's exec */
void exec(const Cmd&);    /* todo: take Pending? */

/* Execute the command, like the operator() */
DeadProc run(const PendingCmd&);

/* Run the command async
   shell:   cmd &
   becomes: cmd.detach() */
Proc detach(const PendingCmd&);

/* Detach but redirect output to a pipe */
Proc detachRedirOut(const PendingCmd&);


/* Run commands in sequence
   shell:
       ls
       cd
   becomes:
       ls,
       cd;
*/
PendingCmd operator,(const PendingCmd&, const Cmd&);
PendingCmd operator,(DeadProc, const Cmd&);
/* Forbiden funcion to prevent wrong sequencing such as:
   echo, echo && echo
   here the 2nd and 3rd echos would be ran before 1st
   due to the C++ operator precedence*/
PendingCmd operator,(const PendingCmd&, DeadProc) = delete;


/* Shell pipe operator | - execute two commands, second takes input
   from first can be chained many times */
PendingCmd operator|(const PendingCmd&, const Cmd&);

/* Shell operator && - run the second command only if first returns 0 (no errors)*/
DeadProc operator&&(const PendingCmd&, const Cmd&);
DeadProc operator&&(DeadProc, const Cmd&);

/* Shell operator || - run second only if first returns != 0
   Operator precedence is different from shell, C precendence is && > ||
   so mixing || and && may not compile, but shouldn't cause other issues,
   use () to resolve these cases */
DeadProc operator||(const PendingCmd&, const Cmd&);
DeadProc operator||(DeadProc, const Cmd&);

/* Shell operator > - redirect output to file
   shell:   cmd > file 2>&1
   becomes: cmd > "file" >=1 */
PendingCmd& operator>(const PendingCmd&, const char* file);
/* A file descriptor can be given istead of a file path in which case
 * no truncation occurs */
PendingCmd& operator>(const PendingCmd&, fd_t);
/* Same but append rather then truncate the file */
PendingCmd& operator>>(const PendingCmd&, const char* file);
PendingCmd& operator>>(const PendingCmd&, fd_t);

/* Redirect errors to file */
PendingCmd& operator>=(const PendingCmd&, const char* file);
PendingCmd& operator>=(const PendingCmd&, fd_t);
/* Same but append rather then truncate the file */
PendingCmd& operator>>=(const PendingCmd&, const char* file);
PendingCmd& operator>>=(const PendingCmd&, fd_t);

/* Shell oprator < use file as input */
PendingCmd& operator<(const PendingCmd&, const char* file);
PendingCmd& operator<(const PendingCmd&, fd_t);

/* Shell operator & but only for single processes
   e.g.: echo a && echo b & echo c
   in shell would detach "echo a && echo b" but here
   this is not possible */
PendingCmd operator&(const PendingCmd&, const Cmd&);

// todo
/* string that is implicitly cast to const char* and doesn't deallocate
   to allow for simpler synthax, speed and safety
   e.g:         echo + some_string
   rather then: echo + some_string.c_str() */
// class str: public std::basic_string<char> /* todo: allocator */
// {
// public:
// 	using std::basic_string<char>::basic_string;
// 	operator const char*() { return c_str();}
// };

/* Execute a command and capture the output, remove trailing newlines like the shell version
   shell:
   var=$(ls)
   becomes:
   auto var = $(ls);
*/
std::string $(const PendingCmd&);

/* Read from a file descriptor until it closes
   can be used on proccess out/err */
std::string read_to_end(fd_t);

/* Operator<< for printing */
std::ostream& operator<<(std::ostream&, const Cmd&);

#include "commands.inl"
