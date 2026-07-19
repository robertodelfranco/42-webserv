#ifndef LISTEN_HPP
# define LISTEN_HPP

#include <string>

class Listen {
	public:
		std::string		host; // 127.0.0.1 ou 0.0.0.0
		unsigned short	port; // porta

		Listen();
		Listen(const std::string& host, int port);
};

#endif
