#ifndef CONFIG_HPP
# define CONFIG_HPP

#include "../Utils/Utils.hpp"
#include "Server.hpp"

class Config {
	private:
		std::ifstream		configFile;
		std::vector<Token>	tokens;
		std::vector<Server>	servers;

		void	consumeLine(const std::string& line);

	public:
		Config();
		Config(const Config& other);
		Config& operator=(const Config& other);
		~Config();

		void	init(const char *file);
};

#endif