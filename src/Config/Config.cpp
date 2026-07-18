#include "Config.hpp"
#include "ConfigLexer.hpp"
#include "ConfigParser.hpp"
#include <iostream>

Config::Config() : servers() {}

Config::~Config() {}

void	Config::load(const char *file) {
	ConfigLexer	lexer;

	if (!lexer.tokenize(file)) {
		std::cerr << "WARNING - Config file '" << file << "' is null, empty, or could not be opened. Using default configuration." << std::endl;
		return; // arquivo nulo/vazio/não abriu
	}

	ConfigParser	parser;
	servers = parser.parse(lexer.getTokens());
}

const std::vector<ServerConfig>&	Config::getServers() const {
	return servers;
}
