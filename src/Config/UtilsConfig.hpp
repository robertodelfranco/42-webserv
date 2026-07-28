#ifndef UTILSCONFIG_HPP
# define UTILSCONFIG_HPP

#include <string>

namespace UtilsConfig {
	void			ref_trim(std::string& str);
	bool			has_char(const char *str, char c);
	bool			has_char(const std::string& str, char c);
	std::string		trim(const std::string& str);
}

#endif
