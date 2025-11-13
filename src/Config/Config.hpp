#ifndef CONFIG_HPP
# define CONFIG_HPP

#include "../Utils/Utils.hpp"
#include "Server.hpp"

class Config {
	private:
		std::ifstream		configFile;
		std::vector<Token>	tokens;
		std::vector<Server>	servers;

		void	consumeLine(std::string& line, size_t count_line);

	public:
		Config();
		Config(const Config& other);
		Config& operator=(const Config& other);
		~Config();

		class ParseError : public std::exception {
			private:
				std::string	m_message;
			public:
				ParseError(const std::string& msg, size_t line, size_t col, const std::string& snippet);
				virtual ~ParseError() throw() {};
				const char* what() const throw();
		};

		void	init(const char *file);
		size_t	consumeIdentifier(const std::string& line, size_t count_line, size_t col);
		size_t	consumeNumber(const std::string& line, size_t count_line, size_t col);
		size_t	consumeString(const std::string& line, size_t count_line, size_t col);
		size_t	consumePath(const std::string& line, size_t count_line, size_t col);
		size_t	consumeSymbol(const std::string& line, size_t count_line, size_t col);
		size_t	edgeCase(const std::string& line, size_t count_line, size_t col);
};

#endif