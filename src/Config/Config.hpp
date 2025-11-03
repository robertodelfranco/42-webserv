#ifndef CONFIG_HPP
# define CONFIG_HPP

#include "../Utils/structs.hpp"
#include "Server.hpp"

class Config {
	private:
		std::vector<Server> servers;

	public:
		Config() : servers() {};
		Config(const Config& other) : servers(other.servers) {};
		Config& operator=(const Config& other) {
			if (this != &other) {
				servers = other.servers;
			}
			return *this;
		};
		~Config() {};

		void	init(char *file) {
			if (!file || !*file) {
				std::cerr << "Nenhum arquivo de configuração fornecido." << std::endl;
				return;
			}

			std::ifstream configFile(file);
			
			if (!configFile.is_open()) {
				std::cerr << "Erro ao abrir o arquivo de configuração: " << file << std::endl;
				return;
			}

			std::string line;
			while (std::getline(configFile, line)) {
				size_t pos = line.find('#');
				if (pos != std::string::npos) {
					line = line.substr(0, pos);
				}
				if (line.find_first_not_of(" \t") != std::string::npos) {
					std::cout << line << std::endl;
				}
			}
		};
};

#endif