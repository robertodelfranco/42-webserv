#include "ConfigParser.hpp"
#include "ConfigParseError.hpp"
#include "UtilsConfig.hpp"
#include "../ServerConfig/Location.hpp"
#include <cctype>
#include <cstdlib>
#include <limits>

ConfigParser::ConfigParser() : handlers(), tokens() {
	handlers[std::string("listen")] = (DirectiveHandler){&ConfigParser::getListen, SERVER, SIMPLE};
	handlers[std::string("client_max_body_size")] = (DirectiveHandler){&ConfigParser::getBodySize, SERVER | LOCATION, SIMPLE};
	handlers[std::string("root")] = (DirectiveHandler){&ConfigParser::getRoot, SERVER | LOCATION, SIMPLE};
	handlers[std::string("index")] = (DirectiveHandler){&ConfigParser::getIndexPage, SERVER | LOCATION, SIMPLE};
	handlers[std::string("error_page")] = (DirectiveHandler){&ConfigParser::getErrorPages, SERVER | LOCATION, SIMPLE};
	handlers[std::string("methods")] = (DirectiveHandler){&ConfigParser::getMethods, LOCATION, SIMPLE};
	handlers[std::string("return")] = (DirectiveHandler){&ConfigParser::getRedirect, LOCATION, SIMPLE};
	handlers[std::string("cgi_type")] = (DirectiveHandler){&ConfigParser::getCgi, LOCATION, SIMPLE};
	handlers[std::string("cgi_path")] = (DirectiveHandler){&ConfigParser::getCgiPath, LOCATION, SIMPLE};
	handlers[std::string("autoindex")] = (DirectiveHandler){&ConfigParser::getAutoindex, LOCATION, SIMPLE};
	handlers[std::string("upload_path")] = (DirectiveHandler){&ConfigParser::getUploadPath, LOCATION, SIMPLE};
	handlers[std::string("location")] = (DirectiveHandler){&ConfigParser::getLocationBlock, SERVER, BLOCK};
}

ConfigParser::~ConfigParser() {}

std::vector<ServerConfig>	ConfigParser::parse(const std::vector<Token>& tokens_param) {
	this->tokens = tokens_param; // atribui ANTES de tirar iteradores -- atribuição em vector pode realocar

	if (tokens.size() < 2)
		throw ConfigParseError("Empty config file", 0, 0, std::string());

	std::vector<ServerConfig>				servers;
	std::vector<Token>::iterator	it = tokens.begin();
	std::vector<Token>::iterator	ite = tokens.end();

	while (it != ite) {
		if (it->type == END_OF_STREAM)
			break ;
		if (it->type == DIRECTIVE && it->value != "server")
			throw ConfigParseError("Directive '" + it->value + "' not allowed outside a server block", it->line, it->col, it->value);
		it = getServerBlock(it, ite, servers); // validate server and then call consume diretive and location block
	}

	return servers;
}

void	ConfigParser::getListen(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it) {
	(void)location_pointer; // listen não é permitido dentro de location
	++it;
	if (it->type != STRING)
		throw ConfigParseError("Invalid listen argument", it->line, it->col, it->value);

	std::string	value = UtilsConfig::trim(it->value);
	std::string	host;
	std::string	port;
	size_t		colon = value.rfind(':');

	if (colon != std::string::npos) {
		host = value.substr(0, colon);
		port = value.substr(colon + 1);
	}
	else
		port = value;

	server.setListen(host, port);

	++it;
}

void	ConfigParser::getBodySize(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it) {
	++it;
	if (it->type != STRING)
		throw ConfigParseError("Invalid body size argument", it->line, it->col, it->value);

	std::string	value = UtilsConfig::trim(it->value);
	size_t		multiplier = 1;

	if (value.empty())
		throw ConfigParseError("Empty body size value", it->line, it->col, it->value);

	char last_char = value[value.size() - 1];
	if (last_char == 'K' || last_char == 'k') {
		multiplier = 1024;
		value = value.substr(0, value.size() - 1);
	}
	else if (last_char == 'M' || last_char == 'm') {
		multiplier = 1024 * 1024;
		value = value.substr(0, value.size() - 1);
	}
	else if (!std::isdigit(last_char)) {
		throw ConfigParseError("Invalid body size suffix", it->line, it->col, it->value);
	}

	if (value.empty())
		throw ConfigParseError("Missing numeric value in body size", it->line, it->col, it->value);

	for (size_t i = 0; i < value.size(); ++i) {
        if (!std::isdigit(value[i]))
            throw ConfigParseError("Invalid body size value", it->line, it->col, it->value);
    }

	long long size = std::atoll(value.c_str());
    if (size < 0)
        throw ConfigParseError("Body size cannot be negative", it->line, it->col, it->value);

    long long max_size = std::numeric_limits<long long>::max() / static_cast<long long>(multiplier);
    if (size > max_size)
        throw ConfigParseError("Body size is too large", it->line, it->col, it->value);

    if (location_pointer != NULL) {
        location_pointer->setBodySize(size * multiplier);
    } else {
        server.setBodySize(size * multiplier);
    }

	++it;
}

void	ConfigParser::getRoot(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it) {
	++it;
	if (it->type != PATH)
		throw ConfigParseError("Invalid root argument", it->line, it->col, it->value);

	if (location_pointer != NULL)
		location_pointer->setRoot(it->value);
	else
		server.setRoot(it->value);

	++it;
}

void	ConfigParser::getIndexPage(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it) {
	++it;
	if (it->type != STRING)
		throw ConfigParseError("Invalid index page argument", it->line, it->col, it->value);

	std::vector<std::string> index_files;
	while (it->type == STRING) {
		std::string value = UtilsConfig::trim(it->value);
		if (value.empty())
			throw ConfigParseError("Empty index page value", it->line, it->col, it->value);

		index_files.push_back(value);
		++it;
	}

	if (location_pointer != NULL)
		location_pointer->setIndexFiles(index_files);
	else
		server.setIndexFiles(index_files);
}

void	ConfigParser::getErrorPages(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it) {
	++it;
	if (it->type != STRING)
		throw ConfigParseError("Invalid error pages argument", it->line, it->col, it->value);

	std::vector<int> error_pages;

	while (it->type == STRING) {
		std::string value = UtilsConfig::trim(it->value);

		if (value.empty())
			throw ConfigParseError("Empty error page value", it->line, it->col, it->value);

		for (size_t i = 0; i < value.size(); ++i) {
			if (!std::isdigit(value[i]))
				throw ConfigParseError("Invalid error code in error page directive", it->line, it->col, it->value);
		}

		if (value.size() > 3)
			throw ConfigParseError("Error code too long in error page directive", it->line, it->col, it->value);

		int error_code = std::atoi(value.c_str());

		if (error_code < 100 || error_code > 599)
			throw ConfigParseError("Error code out of range in error page directive", it->line, it->col, it->value);

		error_pages.push_back(error_code);
		++it;
	}

	if (it->type != PATH)
		throw ConfigParseError("Invalid error page path argument", it->line, it->col, it->value);

	if (location_pointer != NULL)
		location_pointer->setErrorPages(error_pages, it->value);
	else
		server.setErrorPages(error_pages, it->value);

	++it;
}

void	ConfigParser::getMethods(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it) {
	(void)server;
	++it;
	if (it->type != STRING)
		throw ConfigParseError("Invalid methods argument", it->line, it->col, it->value);

	std::vector<std::string> methods;

	while (it->type == STRING) {
		std::string value = UtilsConfig::trim(it->value);
		if (value.empty())
			throw ConfigParseError("Empty method value", it->line, it->col, it->value);

		methods.push_back(value);
		++it;
	}

	location_pointer->setMethods(methods); // garantidamente != NULL, contexto já filtrou isso
}

void	ConfigParser::getRedirect(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it) {
	(void)server; // return é location-only, contexto já garante location_pointer != NULL
	++it;
	if (it->type != STRING)
		throw ConfigParseError("Invalid redirect code argument", it->line, it->col, it->value);

	std::string value = UtilsConfig::trim(it->value);
	if (value.empty())
		throw ConfigParseError("Empty redirect code value", it->line, it->col, it->value);

	if (value.size() > 3)
		throw ConfigParseError("Redirect code too long in return directive", it->line, it->col, it->value);

	for (size_t i = 0; i < value.size(); ++i) {
		if (!std::isdigit(value[i]))
			throw ConfigParseError("Invalid redirect code in return directive", it->line, it->col, it->value);
	}

	size_t	redirect_code = std::atoi(value.c_str());

	if (redirect_code < 300 || redirect_code > 399)
		throw ConfigParseError("Redirect code out of range in return directive", it->line, it->col, it->value);

	++it;
	if (it->type != STRING)
		throw ConfigParseError("Invalid redirect URL argument", it->line, it->col, it->value);

	std::string	redirect_url = UtilsConfig::trim(it->value);

	if (redirect_url.empty())
		throw ConfigParseError("Empty redirect URL value", it->line, it->col, it->value);

	location_pointer->setRedirect(value, redirect_url);
	++it;
}

void	ConfigParser::getCgi(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it) {
	(void)server; // cgi_type é location-only, contexto já garante location_pointer != NULL
	++it;
	if (it->type != PATH)
		throw ConfigParseError("Invalid CGI type argument", it->line, it->col, it->value);

	std::string cgi_extension = UtilsConfig::trim(it->value);

	if (cgi_extension.empty())
		throw ConfigParseError("Empty CGI type value", it->line, it->col, it->value);

	if (cgi_extension != ".py" && cgi_extension != ".php")
		throw ConfigParseError("Unsupported CGI type '" + cgi_extension + "'", it->line, it->col, it->value);

	location_pointer->setCgi(cgi_extension);
	++it;
}

void	ConfigParser::getCgiPath(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it) {
	(void)server; // cgi_path é location-only, contexto já garante location_pointer != NULL
	++it;
	if (it->type != PATH)
		throw ConfigParseError("Invalid CGI path argument", it->line, it->col, it->value);

	std::string cgi_path = UtilsConfig::trim(it->value);

	if (cgi_path.empty())
		throw ConfigParseError("Empty CGI path value", it->line, it->col, it->value);

	location_pointer->setCgiPath(cgi_path);
	++it;
}

void	ConfigParser::getAutoindex(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it) {
	(void)server; // autoindex é location-only, contexto já garante location_pointer != NULL
	++it;
	if (it->type != STRING)
		throw ConfigParseError("Invalid autoindex argument", it->line, it->col, it->value);

	std::string value = UtilsConfig::trim(it->value);

	if (value != "on" && value != "off")
		throw ConfigParseError("Invalid autoindex value '" + value + "' (expected 'on' or 'off')", it->line, it->col, it->value);

	location_pointer->setAutoindex(value == "on");
	++it;
}

void	ConfigParser::getUploadPath(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it) {
	(void)server; // upload_path é location-only, contexto já garante location_pointer != NULL
	++it;
	if (it->type != PATH)
		throw ConfigParseError("Invalid upload path argument", it->line, it->col, it->value);

	std::string path = UtilsConfig::trim(it->value);

	if (path.empty())
		throw ConfigParseError("Empty upload path value", it->line, it->col, it->value);

	location_pointer->setUploadPath(path);
	++it;
}

void	ConfigParser::getLocationBlock(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& start) {
	(void)location_pointer; // sempre NULL aqui -- location block não aninha
	if (start->type != DIRECTIVE || start->value != "location")
		throw ConfigParseError("Expected 'location' directive", start->line, start->col, start->value);
	++start;

	if (start->type != PATH)
		throw ConfigParseError("Invalid location path argument", start->line, start->col, start->value);

	std::string location_path = UtilsConfig::trim(start->value);
	if (location_path.empty())
		throw ConfigParseError("Empty location path value", start->line, start->col, start->value);

	const std::vector<Location>& existing_locations = server.getLocations();
	for (size_t i = 0; i < existing_locations.size(); ++i) {
		if (existing_locations[i].getPath() == location_path)
			throw ConfigParseError("Duplicate location path '" + location_path + "'", start->line, start->col, start->value);
	}
	++start;

	if (start->type != SYMBOL || start->value != "{")
		throw ConfigParseError("Expected open brace after location path", start->line, start->col, start->value);
	++start;

	Location	location;
	location.setPath(location_path); // seta o path do location

	while (start != tokens.end()) {
		if (start->type == SYMBOL && start->value == "}")
			break ;

		if (start->type != DIRECTIVE)
			throw ConfigParseError("Syntax error in location block", start->line, start->col, start->value);

		getDirective(server, &location, start, LOCATION); // cada diretiva de location vai ser consumida totalmente aqui dentro, sem perigo de "vazar" uma close brace pra condição abaixo
	}

	if (start == tokens.end())
		throw ConfigParseError("Expected closing brace for location block", tokens.back().line, tokens.back().col, std::string());

	location.mergeDefaults(server); // preenche root/index/error_page/client_max_body_size que o location não declarou com o valor do server
	server.addLocation(location); // única cópia, adiciona o location ao vetor de locations do server
}

void	ConfigParser::consumeSemiColon(std::vector<Token>::iterator& it) {
	if (it->type != SYMBOL || it->value != ";")
		throw ConfigParseError("Expected semi colon at the end of line", it->line, it->col, it->value);
	++it;
}

void	ConfigParser::consumeRightBrace(std::vector<Token>::iterator&it) {
	if (it->type != SYMBOL || it->value != "}")
		throw ConfigParseError("Expected right brace at the end of block location", it->line, it->col, it->value);
	++it;
}

void	ConfigParser::getDirective(ServerConfig& server, Location* location_pointer, std::vector<Token>::iterator& it, DirectiveContext context) {
	std::map<std::string, DirectiveHandler>::iterator function = handlers.find(it->value);

	if (function == handlers.end())
		throw ConfigParseError("Unknown directive", it->line, it->col, it->value);

	if (!(function->second.context & context))
		throw ConfigParseError("Directive '" + it->value + "' not allowed in this context", it->line, it->col, it->value);

	(this->*(function->second.handler))(server, location_pointer, it); // cada função consome a linha toda e avança o iterator até o token ';' ou '}' no caso do location

	if (function->second.kind == SIMPLE)
		consumeSemiColon(it); // aqui vai uma função "expect" para consumir o token esperado ';' ou '}' no caso do location
	else
		consumeRightBrace(it); // aqui vai uma função "expect" para consumir o token esperado ';' ou '}' no caso do location
}

std::vector<Token>::iterator	ConfigParser::getServerBlock(std::vector<Token>::iterator& start, std::vector<Token>::iterator end, std::vector<ServerConfig>& servers) {
	size_t	server_line = start->line;
	size_t	server_col = start->col;

	++start;
	if (start->value != "{")
		throw ConfigParseError("Syntax error, expected open brace after directive server", start->line, start->col, start->value);

	++start;
	if (start->value == "}")
		throw ConfigParseError("Syntax error, server block empty", start->line, start->col, start->value);

	ServerConfig	server;

	while (start != end) {
		if (start->type != DIRECTIVE)
			throw ConfigParseError("Syntax error", start->line, start->col, start->value);

		getDirective(server, NULL, start, SERVER); // location block vai ser consumido totalmente aqui dentro, sem perigo de "vazar" uma close brace pra condição abaixo e NULL = não estamos em location nenhum

		if (start->type == SYMBOL && start->value == "}") {
			++start;
			break ;
		}
	}

	if (server.getListens().empty())
		throw ConfigParseError("Server block must have at least one 'listen' directive", server_line, server_col, std::string());

	if (server.getRoot().empty())
		throw ConfigParseError("Server block must have a 'root' directive", server_line, server_col, std::string());

	servers.push_back(server);
	return start;
}
