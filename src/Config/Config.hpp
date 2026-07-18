#ifndef CONFIG_HPP
# define CONFIG_HPP

#include <exception>
#include "../Utils/Utils.hpp"
#include "Token.hpp"
#include "Server.hpp"

enum DirectiveKind {
	SIMPLE,
	BLOCK
};

enum DirectiveContext {
	SERVER = 1, // 1 << 0
	LOCATION = 2 // 1 << 1
};

class Config;
typedef void (Config::*TokenHandler)(Server&, Location*, std::vector<Token>::iterator&); // protótipo para declarar array de funções

struct DirectiveHandler {
	TokenHandler	handler;
	size_t			context;
	DirectiveKind	kind;
};

class Config {
	private:
		std::vector<Server>						servers; // blocos de servidores que reunem todos os dados do arquivo config
		std::vector<Token>						tokens;
		std::map<std::string, DirectiveHandler>	handlers;

		// Parser
		std::vector<Token>::iterator	getServerBlock(std::vector<Token>::iterator& start, std::vector<Token>::iterator end);
		void							consumeSemiColon(std::vector<Token>::iterator& it);
		void							consumeRightBrace(std::vector<Token>::iterator& it);
		void							getDirective(Server& server, Location* location_pointer, std::vector<Token>::iterator& it, DirectiveContext context);
		void							getListen(Server& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getServerName(Server& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getBodySize(Server& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getRoot(Server& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getIndexPage(Server& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getErrorPages(Server& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getMethods(Server& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getRedirect(Server& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getCgi(Server& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getCgiPath(Server& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getLocationBlock(Server& server, Location* location_pointer, std::vector<Token>::iterator& start);

		// Lexer
		void							consumeLine(std::string& line, size_t count_line);
		size_t							consumeDirective(const std::string& line, size_t count_line, size_t col);
		size_t							consumeName(const std::string& line, size_t count_line, size_t col);
		size_t							consumeString(const std::string& line, size_t count_line, size_t col);
		size_t							consumePath(const std::string& line, size_t count_line, size_t col);
		size_t							consumeSymbol(const std::string& line, size_t count_line, size_t col);
		size_t							edgeCase(const std::string& line, size_t count_line, size_t col);

	public:
		Config();
		~Config();

		class ParseError : public std::exception {
			private:
				std::string	m_message;
			public:
				ParseError(const std::string& msg, size_t line, size_t col, const std::string& snippet);
				virtual ~ParseError() throw() {};
				const char* what() const throw();
		};

		void	initLexer(const char *file);
		void	initParser();

		const std::vector<Server>&	getServers() const;
};

#endif