#ifndef COLOR_HPP
# define COLOR_HPP

#include <string>

namespace Color {
	const std::string WHITEBOLD = "\033[1;37m";
	const std::string YELLOW    = "\033[1;33m";
	const std::string GREEN     = "\033[1;32m";
	const std::string CYAN      = "\033[1;36m";
	const std::string RED       = "\033[1;31m";
	const std::string RESET     = "\033[0m";
}

#endif
