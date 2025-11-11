#include "Config.hpp"

Config::Config() : servers() {};

Config::Config(const Config& other) : servers(other.servers) {};

Config& Config::operator=(const Config& other) {
	if (this != &other) {
		servers = other.servers;
	}
	return *this;
};

Config::~Config() {};

void	Config::init(const char *file) {
	if (!file || !*file) {
		std::cerr << "Nenhum arquivo de configuração foi encontrado" << std::endl;
		return;
	}

	std::ifstream	configFile(file);
	
	if (!configFile) {
		std::cerr << "Erro ao abrir o arquivo de configuração: " << file << std::endl;
		return;
	}

	std::string line;
	size_t	count_lines = 0;
	while (std::getline(configFile, line)) {
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
	configFile.close();
};

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
		if (std::isalpha(c) || c == '_') {
			col = consumeIdentifier(line, col);
		}
		else if (std::isdigit(c)) {
			col = consumeNumber(line, col);
		}
		else if (c == '\"' || c == '\'') {
			col = consumeString(line, col);
		}
		else if (c == '/' || c == '.') {
			col = consumePath(line, col);
		}
		else if (c == '{' || c == '}' || c == ';')
			col = consumeSymbol(line, col);
		else
			col = edgeCase(line, col);
	}
}
