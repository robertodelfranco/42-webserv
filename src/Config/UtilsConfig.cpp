#include "UtilsConfig.hpp"
#include <cctype>

std::string	UtilsConfig::trim(const std::string& str) {
	size_t	first = str.find_first_not_of(" \t\n\r\f\v");

	if (first == std::string::npos)
		return std::string();

	size_t	last = str.find_last_not_of(" \t\n\r\f\v");

	return str.substr(first, last - first + 1);
}

void	UtilsConfig::ref_trim(std::string& str) {
	size_t	n = str.size();
	size_t	start = 0;

	while (start < n && isspace(static_cast<unsigned char>(str[start])))
		++start;

	if (start == n) {
		str.clear();
		return;
	}

	size_t	end = n - 1;
	while (end > start && isspace(static_cast<unsigned char>(str[end])))
		--end;

	str = str.substr(start, end - start + 1);
}

bool	UtilsConfig::has_char(const char *str, char c) {
	if (str == NULL) return false;

	for (const char *p = str; *p != '\0'; ++p)
		if (static_cast<unsigned char>(*p) == c) return true;

	return false;
}

bool	UtilsConfig::has_char(const std::string& str, char c) {
	return (str.find(c) != std::string::npos);
}

bool	UtilsConfig::isAllDigits(const std::string& str) {
	if (str.empty())
		return false;

	for (size_t i = 0; i < str.size(); ++i)
		if (!std::isdigit(static_cast<unsigned char>(str[i])))
			return false;

	return true;
}
