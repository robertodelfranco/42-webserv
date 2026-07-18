#ifndef CONFIGPARSEERROR_HPP
# define CONFIGPARSEERROR_HPP

#include <exception>
#include <string>
#include <cstddef>

// Erro de sintaxe/semântica do arquivo de config, lançado tanto pelo
// ConfigLexer (fase de tokenização) quanto pelo ConfigParser (fase de
// parsing).
class ConfigParseError : public std::exception {
	private:
		std::string	m_message;

	public:
		ConfigParseError(const std::string& msg, size_t line, size_t col, const std::string& snippet);
		virtual ~ConfigParseError() throw() {};
		const char*	what() const throw();
};

#endif
