cppipe - C Plus Pipe
====================
cppipe is a utility for Linux that allows you to use C/C++ programs as scripts.  
It also offers a shell like syntax for seamless command execution

In this way your scripts can benefit from increased control and speed, while
maintaining a lot of the potential for simple command call syntax.

C/C++ programs are cached and only recompiled if needed.


Installation
------------
git clone https://gitlab.com/venelin98/cppipe.git  
cd cppipe  
sudo ./install.sh


How to execute a C/C++ source file
----------------------------------
cppipe PATH_TO_SRC [ARGUMENTS_FOR_YOUR_PROGRAM]...


Example
-------
You can see an example of the command call syntax in test/functions_test.cppipe


Webpage with more info
----------------------
https://vene.volconst.com/en/cppipe
