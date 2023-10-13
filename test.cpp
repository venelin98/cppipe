#include <cppipe/commands.hpp>

#include <sys/wait.h>
#include <iostream>

// shaded obj?
/* close files on destruction ?*/
using namespace std;
int main()
{
	Cmd ls("ls");
	Cmd ll("ls", "-l");
	Cmd grep("grep");
	Cmd echo("echo");
	Cmd rm("rm");

	string out1 = $(ll | grep + "cpp" | grep + "child");
	if(out1.find("childProcess.cpp") != string::npos)
		cout << "OK 0/11" << endl;

	Cmd success("echo", "OK 1/11");
	Cmd fail("mkdir", ".");
	Cmd unexpected("echo", "FAILURE");
	success &&
	fail &&
	unexpected;

	// shouldnt compile
	// success ||
	// unexpected &&
	// unexpected;

	Cmd write_file("echo", "Existing ", " ", "file. OK 2/11");

	write_file > "file.txt";
	grep + "Existing" < "file.txt";

	echo + "Appended to file OK 3/11" >> "file.txt";
	grep + "Appended" < "file.txt" &&
	rm + "file.txt" &&
	fail ||
	Cmd("echo", "OK 4/11");

	echo + "OK 5/11" &&
	echo + "OK 6/11",
	Cmd("echo", "OK 7/11");

	echo + "OK 8/11" &
	echo + "OK 9/11" &&
	echo + "OK 10/11";

	// wait for all detached
	while(wait(nullptr) != -1);

	// todo
	// exec( echo + $(echo + "OK 11/11") );
}
