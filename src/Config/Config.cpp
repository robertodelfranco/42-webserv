#include "Config.hpp"
#include "ConfigLexer.hpp"
#include "ConfigParser.hpp"
#include "../Utils/Logger.hpp"
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
	Logger::info() << "Config OK: " << servers.size() << " server(s)";
	for (size_t i = 0; i < servers.size(); ++i) {
		const std::vector<Listen>& l = servers[i].getListens();
		for (size_t j = 0; j < l.size(); ++j)
			Logger::debug() << "  server[" << i << "] listen " << l[j].host << ":" << l[j].port;
}
}

const std::vector<ServerConfig>&	Config::getServers() const {
	return servers;
}