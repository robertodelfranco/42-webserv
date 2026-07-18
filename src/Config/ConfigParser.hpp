#ifndef CONFIGPARSER_HPP
# define CONFIGPARSER_HPP

#include <vector>
#include <map>
#include <string>
#include "Token.hpp"
#include "../ServerConfig/ServerConfig.hpp"

enum DirectiveKind {
	SIMPLE,
	BLOCK
};

enum DirectiveContext {
	SERVER = 1, // 1 << 0
	LOCATION = 2 // 1 << 1
};

class ConfigParser;
typedef void (ConfigParser::*TokenHandler)(ServerConfig&, Location*, std::vector<Token>::iterator&); // protótipo para declarar array de funções

struct DirectiveHandler {
	TokenHandler	handler;
	size_t			context;
	DirectiveKind	kind;
};

// Fase 2 do parsing de config: vector<Token> vira vector<ServerConfig>. Consome a
// sequência de tokens já reconhecida pelo ConfigLexer.
class ConfigParser {
	private:
		std::map<std::string, DirectiveHandler>	handlers;
		std::vector<Token>							tokens; // cópia do que parse() recebeu -- getLocationBlock precisa de tokens.end() e não pode ganhar esse parâmetro (assinatura presa ao TokenHandler)

		std::vector<Token>::iterator	getServerBlock(std::vector<Token>::iterator& start, std::vector<Token>::iterator end, std::vector<ServerConfig>& servers);
		void							consumeSemiColon(std::vector<Token>::iterator& it);
		void							consumeRightBrace(std::vector<Token>::iterator& it);
		void							getDirective(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it, DirectiveContext context);
		void							getListen(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getServerName(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getBodySize(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getRoot(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getIndexPage(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getErrorPages(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getMethods(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getRedirect(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getCgi(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getCgiPath(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it);
		void							getLocationBlock(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& start);

	public:
		ConfigParser();
		~ConfigParser();

		std::vector<ServerConfig>	parse(const std::vector<Token>& tokens);
};

#endif
