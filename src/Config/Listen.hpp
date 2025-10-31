#ifndef LISTEN_HPP
# define LISTEN_HPP

#include <string>

struct Listen {
	std::string		host; // 127.0.0.1 ou 0.0.0.0
	unsigned short	port; // porta
};

#endif