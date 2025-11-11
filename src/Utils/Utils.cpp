#include "Utils.hpp"

Utils::Utils() {};

Utils::Utils(const Utils& other) {
	(void)other;
}

Utils&	Utils::operator=(const Utils& other) {
	(void)other;
	return *this;
}

Utils::~Utils() {};

std::string	Utils::trim(const std::string& str) {
	size_t	first = str.find_first_not_of(" \t\n\r\f\v");
	
	if (first == std::string::npos)
		return std::string();
	
	size_t	last = str.find_last_not_of(" \t\n\r\f\v");

	return str.substr(first, last - first + 1);
}

void	Utils::ref_trim(std::string& str) {
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
