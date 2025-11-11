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
	while (std::getline(configFile, line)) {
		size_t pos = line.find('#');
		if (pos != std::string::npos) {
			line = line.substr(0, pos);
		}
		Utils::ref_trim(line);
		if (line.length() > 0) {
			std::cout << YELLOW << line << " |" << RESET << std::endl;
			consumeLine(line);
		}
	}

	configFile.close();
};

void	Config::consumeLine(const std::string& line) {
	(void)line;
}
