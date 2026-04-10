#include "Config.hpp"

Config::Config() : servers(), tokens(), handlers() {
	handlers[std::string("listen")] = (DirectiveHandler){&Config::getListen, SERVER, SIMPLE};
	handlers[std::string("server_name")] = (DirectiveHandler){&Config::getServerName, SERVER, SIMPLE};
	handlers[std::string("client_max_body_size")] = (DirectiveHandler){&Config::getBodySize, SERVER | LOCATION, SIMPLE};
	handlers[std::string("root")] = (DirectiveHandler){&Config::getRoot, SERVER | LOCATION, SIMPLE};
	handlers[std::string("index")] = (DirectiveHandler){&Config::getIndexPage, SERVER | LOCATION, SIMPLE};
	handlers[std::string("error_page")] = (DirectiveHandler){&Config::getErrorPages, SERVER | LOCATION, SIMPLE};
	handlers[std::string("methods")] = (DirectiveHandler){&Config::getMethods, LOCATION, SIMPLE};
	handlers[std::string("return")] = (DirectiveHandler){&Config::getRedirect, LOCATION, SIMPLE};
	handlers[std::string("cgi_type")] = (DirectiveHandler){&Config::getCgi, LOCATION, SIMPLE};
	handlers[std::string("cgi_path")] = (DirectiveHandler){&Config::getCgiPath, LOCATION, SIMPLE};
	handlers[std::string("location")] = (DirectiveHandler){&Config::getLocationBlock, SERVER, BLOCK};
}

Config::~Config() {}

Config::ParseError::ParseError(const std::string& msg, size_t line, size_t col, const std::string& snippet) {
	std::ostringstream	os;

	os << "Parse error: " << msg << " (line: " << line << " col: " << (col == 0 ? col : col - 1) << ")";
	if (!snippet.empty())
		os << "\n" << snippet << "\n" << RED << std::string((col == 0 ? col : col - 1), ' ') << '^' << RESET;

	m_message = os.str();
};

const char*	Config::ParseError::what() const throw() {
	return m_message.c_str();
}

static bool	is_directive_char(unsigned char c) {
	if (std::isalpha(c)) return true;
	if (std::isdigit(c)) return true;
	if (c == '_') return true;

	return false;
}

size_t	Config::consumeDirective(const std::string& line, size_t count_line, size_t col) {
	if (col >= line.size())
		return col;

	unsigned char	c = static_cast<unsigned char>(line[col]);
	size_t		start = col;

	if (!is_directive_char(c))
		return col;
	++col;
	
	while (col < line.size()) {
		c = static_cast<unsigned char>(line[col]);
		
		if (Utils::has_char(" \t{;}", c))
			break ;
		
		if (c == '\'' || c == '"')
			throw ParseError("Invalid quotes in directive", count_line, col, line);

		if (!is_directive_char(c))
			break ;
		
		++col;
	}

	if (col == start)
		return col;
	
	tokens.push_back(Token(DIRECTIVE, line.substr(start, col - start), count_line, start + 1));
	
	return col;
}

size_t	Config::consumeName(const std::string& line, size_t count_line, size_t col) {
	if (col >= line.size())
		return col;

	size_t		start = col;

	while (col < line.size()) {
		unsigned char	c = static_cast<unsigned char>(line[col]);

		if (Utils::has_char(" \t{;}", c))
			break ;
	
		if (c == '\'' || c == '"')
			throw ParseError("Invalid quotes in name", count_line, col, line);
		
		if (c == '\0' && c < 32)
			throw ParseError("Unexpected control character in name", count_line, col, line);

		++col;
	}

	tokens.push_back(Token(STRING, line.substr(start, col - start), count_line, start + 1));
	
	return col;
}

size_t	Config::consumeString(const std::string& line, size_t count_line, size_t col) {
	if (col >= line.size())
		return col;

	unsigned char	quote = static_cast<unsigned char>(line[col]);
	if (quote != '"' && quote != '\'')
		return col;
	
	size_t		start = col;
	++col;
	
	std::string content;
	while (col < line.size()) {
		unsigned char	c = static_cast<unsigned char>(line[col]);

		if (c == '\\') {
			++col;
			if (col >= line.size())
				throw ParseError("Invalid escape at the end of line", count_line, col, line);
			content.push_back(line[col]);
			++col;
			continue ;
		}

		if (c == quote) {
			++col;
			
			if (content.empty())
				throw ParseError("Empty string", count_line, col, line);

			tokens.push_back(Token(STRING, content, count_line, start + 1));
			return col;
		}
		content.push_back(line[col]);
		++col;
	}

	throw ParseError("Unclosed quotes", count_line, col, line);
}

size_t	Config::consumePath(const std::string& line, size_t count_line, size_t col) {
	if (col >= line.size())
		return col;

	unsigned char	c = static_cast<unsigned char>(line[col]);
	size_t		start = col;

	if (Utils::has_char(" \t{;}", c))
		return col;
	++col;

	while (col < line.size()) {
		c = static_cast<unsigned char>(line[col]);

		if (Utils::has_char(" \t{;}", c))
			break ;
		
		if (c == '\'' || c == '"')
			throw ParseError("Invalid quotes in path", count_line, col, line);

		if (c < 32)
			throw ParseError("Unexpected control character in path", count_line, col, line);

		++col;
	}

	tokens.push_back(Token(PATH, line.substr(start, col - start), count_line, start + 1));

	return col;
}

size_t	Config::consumeSymbol(const std::string& line, size_t count_line, size_t col) {
	if (col >= line.size())
		return col;

	unsigned char	c = static_cast<unsigned char>(line[col]);
	size_t		start =	col;

	if (!Utils::has_char("{;}", c))
		return col;
	++col;

	tokens.push_back(Token(SYMBOL, line.substr(start, col - start), count_line, start + 1));

	return col;
}

size_t	Config::edgeCase(const std::string& line, size_t count_line, size_t col) {
	if (col >= line.size())
		return col;
	
	size_t		start = col;
		
	while (col < line.size()) {
		unsigned char	c = static_cast<unsigned char>(line[col]);

		if (Utils::has_char(" \t{;}", c))
			break ;

		++col;
	}

	tokens.push_back(Token(EDGE_CASE, line.substr(start, col - start), count_line, start + 1));

	return col;
}

void	Config::consumeLine(std::string& line, size_t count_line) {

	if (line.empty())
		return ;

	size_t	col = 0;
	while (col < line.size()) {
		unsigned char c = static_cast<unsigned char>(line[col]);

		if (std::isspace(c)) {
			++col;
			continue ;
		}
		if (col == 0 && (std::isalpha(c) || c == '_'))
			col = consumeDirective(line, count_line, col);
		else if (std::isalpha(c) || c == '_' || std::isdigit(c))
			col = consumeName(line, count_line, col);
		else if (c == '\"' || c == '\'')
			col = consumeString(line, count_line, col);
		else if (c == '/' || c == '.')
			col = consumePath(line, count_line, col);
		else if (c == '{' || c == '}' || c == ';')
			col = consumeSymbol(line, count_line, col);
		else
			col = edgeCase(line, count_line, col);
	}
}

void	Config::initLexer(const char *file) {
	if (!file || !*file) {
		std::cerr << "Nenhum arquivo de configuração foi encontrado" << std::endl;
		return;
	}

	std::ifstream	config_file(file);
	
	if (!config_file) {
		std::cerr << "Erro ao abrir o arquivo de configuração: " << file << std::endl;
		return;
	}

	std::string line;
	size_t	count_lines = 1;
	while (std::getline(config_file, line)) {
		size_t pos = line.find('#');
		if (pos != std::string::npos) {
			line = line.substr(0, pos);
		}
		Utils::ref_trim(line);
		if (line.length() > 0) {
			std::cout << YELLOW << line << " |" << RESET << std::endl;
			consumeLine(line, count_lines);
		}
		++count_lines;
	}
	tokens.push_back(Token(END_OF_STREAM, "EOF", count_lines, 0));
	config_file.close();

	std::cout << std::endl;
	for (std::vector<Token>::iterator it = tokens.begin(); it != tokens.end(); ++it) {
		std::cout << GREEN << it->type << " -- " << it->value << "\n" << it->line << " -- " << it->col << RESET << std::endl;
		std::cout << std::endl;
	}

	initParser();
}

void	Config::getListen(Server& server, std::vector<Token>::iterator& it) {
	++it;
	if (it->type != STRING)
		throw ParseError("Invalid listen argument", it->line, it->col, it->value);

	std::string	value = Utils::trim(it->value);
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

void	Config::getServerName(Server& server, std::vector<Token>::iterator& it) {
	(void)server;
	(void)it;
}

void	Config::getBodySize(Server& server, std::vector<Token>::iterator& it) {
	++it;
	if (it->type != STRING)
		throw ParseError("Invalid body size argument", it->line, it->col, it->value);
	
	std::string	value = Utils::trim(it->value);
	size_t		multiplier = 1;

	if (value.empty())
		throw ParseError("Empty body size value", it->line, it->col, it->value);

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
		throw ParseError("Invalid body size suffix", it->line, it->col, it->value);
	}

	if (value.empty())
		throw ParseError("Missing numeric value in body size", it->line, it->col, it->value);

	for (size_t i = 0; i < value.size(); ++i) {
        if (!std::isdigit(value[i]))
            throw ParseError("Invalid body size value", it->line, it->col, it->value);
    }

	long long size = std::atoll(value.c_str());
    if (size < 0)
        throw ParseError("Body size cannot be negative", it->line, it->col, it->value);

    long long max_size = std::numeric_limits<long long>::max() / static_cast<long long>(multiplier);
    if (size > max_size)
        throw ParseError("Body size is too large", it->line, it->col, it->value);

    server.setBodySize(size * multiplier);

	++it;
}

void	Config::getRoot(Server& server, std::vector<Token>::iterator& it) {
	++it;
	if (it->type != PATH)
		throw ParseError("Invalid root argument", it->line, it->col, it->value);

	server.setRoot(it->value);

	++it;
}

void	Config::getIndexPage(Server& server, std::vector<Token>::iterator& it) {
	++it;
	if (it->type != STRING)
		throw ParseError("Invalid index page argument", it->line, it->col, it->value);
	
	std::vector<std::string> index_files;
	while (it->type == STRING) {
		std::string value = Utils::trim(it->value);
		if (value.empty())
			throw ParseError("Empty index page value", it->line, it->col, it->value);
		
		index_files.push_back(value);
		++it;
	}

	server.setIndexFiles(index_files);
}

void	Config::getErrorPages(Server& server, std::vector<Token>::iterator& it) {
	++it;
	if (it->type != STRING)
		throw ParseError("Invalid error pages argument", it->line, it->col, it->value);

	std::vector<int> error_pages;

	while (it->type == STRING) {
		std::string value = Utils::trim(it->value);
		
		if (value.empty())
			throw ParseError("Empty error page value", it->line, it->col, it->value);
		
		for (size_t i = 0; i < value.size(); ++i) {
			if (!std::isdigit(value[i]))
				throw ParseError("Invalid error code in error page directive", it->line, it->col, it->value);
		}

		if (value.size() > 3)
			throw ParseError("Error code too long in error page directive", it->line, it->col, it->value);
		
		int error_code = std::atoi(value.c_str());

		if (error_code < 100 || error_code > 599)
			throw ParseError("Error code out of range in error page directive", it->line, it->col, it->value);
		
		error_pages.push_back(error_code);
		++it;
	}
	
	if (it->type != PATH)
		throw ParseError("Invalid error page path argument", it->line, it->col, it->value);

	server.setErrorPages(error_pages, it->value);

	++it;
}

void	Config::getMethods(Server& server, std::vector<Token>::iterator& it) {
	++it;
	if (it->type != STRING)
		throw ParseError("Invalid methods argument", it->line, it->col, it->value);
	
	std::vector<std::string> methods;

	while (it->type == STRING) {
		std::string value = Utils::trim(it->value);
		if (value.empty())
			throw ParseError("Empty method value", it->line, it->col, it->value);
		
		methods.push_back(value);
		++it;
	}

	server.setMethods(methods);
}

void	Config::getRedirect(Server& server, std::vector<Token>::iterator& it) {
	++it;
	if (it->type != STRING)
		throw ParseError("Invalid redirect code argument", it->line, it->col, it->value);
	
	std::string value = Utils::trim(it->value);
	if (value.empty())
		throw ParseError("Empty redirect code value", it->line, it->col, it->value);
	
	if (value.size() > 3)
		throw ParseError("Redirect code too long in return directive", it->line, it->col, it->value);
	
	for (size_t i = 0; i < value.size(); ++i) {
		if (!std::isdigit(value[i]))
			throw ParseError("Invalid redirect code in return directive", it->line, it->col, it->value);
	}

	size_t	redirect_code = std::atoi(value.c_str());

	if (redirect_code < 300 || redirect_code > 399)
		throw ParseError("Redirect code out of range in return directive", it->line, it->col, it->value);
	
	++it;
	if (it->type != STRING)
		throw ParseError("Invalid redirect URL argument", it->line, it->col, it->value);
	
	std::string	redirect_url = Utils::trim(it->value);

	if (redirect_url.empty())
		throw ParseError("Empty redirect URL value", it->line, it->col, it->value);
	
	server.setRedirect(value, redirect_url);
	++it;
}

void	Config::getCgi(Server& server, std::vector<Token>::iterator& it) {
	++it;
	if (it->type != PATH)
		throw ParseError("Invalid CGI type argument", it->line, it->col, it->value);
	
	std::string cgi_extension = Utils::trim(it->value);

	if (cgi_extension.empty())
		throw ParseError("Empty CGI type value", it->line, it->col, it->value);
	
	if (cgi_extension != ".py" && cgi_extension != ".php")
		throw ParseError("Unsupported CGI type '" + cgi_extension + "'", it->line, it->col, it->value);
	
	server.setCgi(cgi_extension);
	++it;
}

void	Config::getCgiPath(Server& server, std::vector<Token>::iterator& it) {
	++it;
	if (it->type != PATH)
		throw ParseError("Invalid CGI path argument", it->line, it->col, it->value);
	
	std::string cgi_path = Utils::trim(it->value);

	if (cgi_path.empty())
		throw ParseError("Empty CGI path value", it->line, it->col, it->value);
	
	server.setCgiPath(cgi_path);
	++it;
}

void	Config::getLocationBlock(Server& server, std::vector<Token>::iterator& start) {
	if (start->type != DIRECTIVE || start->value != "location")
		throw ParseError("Expected 'location' directive", start->line, start->col, start->value);
	++start;

	if (start->type != PATH)
		throw ParseError("Invalid location path argument", start->line, start->col, start->value);
	++start;

	if (start->type != SYMBOL || start->value != "{")
		throw ParseError("Expected open brace after location path", start->line, start->col, start->value);
	++start;

	Location	location;
	server.setLocation(location); // adiciona o location vazio no server e seta o ponteiro current_location para ele, para as próximas diretivas de location setarem os dados nesse location

	while (start != tokens.end()) {
		if (start->type == SYMBOL && start->value == "}")
			break ;
		
		if (start->type != DIRECTIVE)
			throw ParseError("Syntax error in location block", start->line, start->col, start->value);
		
		getDirective(server, start, LOCATION); // cada diretiva de location vai ser consumida totalmente aqui dentro, sem perigo de "vazar" uma close brace pra condição abaixo
	}

	if (start == tokens.end())
		throw ParseError("Expected closing brace for location block", start->line, start->col, start->value);

	server.unsetLocation(); // desseta o ponteiro current_location do server para evitar que diretivas fora do location setem dados no location por engano
}

void	Config::consumeSemiColon(std::vector<Token>::iterator& it) {
	if (it->type != SYMBOL || it->value != ";")
		throw ParseError("Expected semi colon at the end of line", it->line, it->col, it->value);
	++it;
}

void	Config::consumeRightBrace(std::vector<Token>::iterator&it) {
	if (it->type != SYMBOL || it->value != "}")
		throw ParseError("Expected right brace at the end of block location", it->line, it->col, it->value);
	++it;
}

void	Config::getDirective(Server& server, std::vector<Token>::iterator& it, DirectiveContext context) {
	std::map<std::string, DirectiveHandler>::iterator function = handlers.find(it->value);

	if (function == handlers.end())
		throw ParseError("Unknown directive", it->line, it->col, it->value);

	if (!(function->second.context & context))
		throw ParseError("Directive '" + it->value + "' not allowed in this context", it->line, it->col, it->value);

	(this->*(function->second.handler))(server, it); // cada função consome a linha toda e avança o iterator até o token ';' ou '}' no caso do location

	if (function->second.kind == SIMPLE)
		consumeSemiColon(it); // aqui vai uma função "expect" para consumir o token esperado ';' ou '}' no caso do location
	else
		consumeRightBrace(it); // aqui vai uma função "expect" para consumir o token esperado ';' ou '}' no caso do location
}

std::vector<Token>::iterator	Config::getServerBlock(std::vector<Token>::iterator& start, std::vector<Token>::iterator end) {

	++start;
	if (start->value != "{")
		throw ParseError("Syntax error, expected open brace after directive server", start->line, start->col, start->value);

	++start;
	if (start->value == "}")
		throw ParseError("Syntax error, server block empty", start->line, start->col, start->value);

	Server	server;
		
	while (start != end) {
		if (start->type != DIRECTIVE)
			throw ParseError("Syntax error", start->line, start->col, start->value);

		getDirective(server, start, SERVER); // location block vai ser consumido totalmente aqui dentro, sem perigo de "vazar" uma close brace pra condição abaixo

		if (start->type == SYMBOL && start->value == "}") {
			++start;
			break ;
		}
	}
	servers.push_back(server);
	return start;
}

void	Config::initParser() {
	if (tokens.size() < 2)
		throw ParseError("Empty config file", 0, 0, std::string());
	
	std::vector<Token>::iterator it = tokens.begin();
	std::vector<Token>::iterator ite = tokens.end();

	while (it != ite) {
		if (it->type == END_OF_STREAM)
			break ;
		if (it->type == DIRECTIVE && it->value != "server")
			throw ParseError("WARNING - directive out of server block is ignored", it->line, it->col, it->value);
		it = getServerBlock(it, ite); // validate server and then call consume diretive and location block
	}
}
