#include "Config.hpp"
#include "ConfigLexer.hpp"
#include "ConfigParser.hpp"
#include <stdexcept>

Config::Config() : servers() {}

Config::~Config() {}

void	Config::load(const char *file) {
	ConfigLexer	lexer;

	if (!lexer.tokenize(file)) {
		std::string name = (file && *file) ? file : "(nenhum arquivo informado)";
		throw std::runtime_error("Config: não foi possível abrir o arquivo de configuração '" + name + "'");
	}

	ConfigParser	parser;
	servers = parser.parse(lexer.getTokens());
}

const std::vector<ServerConfig>&	Config::getServers() const {
	return servers;
}