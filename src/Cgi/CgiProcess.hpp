#ifndef CGIPROCESS_HPP
# define CGIPROCESS_HPP

#include "../Network/FileDescriptor.hpp"
#include <unistd.h>
#include <iostream>

class CgiProcess {
	private:
		pid_t			_pid;
		FileDescriptor	_stdin;
		FileDescriptor	_stdout;
		std::string		_body;
		size_t			_bodyOffset;
		int				_exitstatus;
	
	public:
		CgiProcess();
		~CgiProcess();
	
		bool	start(); // bool pra caso falhe o fork/pipe upa erro 500
};

#endif