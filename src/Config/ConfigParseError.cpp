#include "ConfigParseError.hpp"
#include "Color.hpp"
#include <sstream>

ConfigParseError::ConfigParseError(const std::string& msg, size_t line, size_t col, const std::string& snippet) {
	std::ostringstream	os;

	os << "Parse error: " << msg << " (line: " << line << " col: " << (col == 0 ? col : col - 1) << ")";
	if (!snippet.empty())
		os << "\n" << snippet << "\n" << Color::RED << std::string((col == 0 ? col : col - 1), ' ') << '^' << Color::RESET;

	m_message = os.str();
}

const char*	ConfigParseError::what() const throw() {
	return m_message.c_str();
}
