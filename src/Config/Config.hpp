#ifndef CONFIG_HPP
# define CONFIG_HPP

#include "../Utils/structs.hpp"
#include "Server.hpp"

struct Token {
	TokenType	type;
	std::string	value;
	int			line;
	int			col;
};

class Config {
	private:
		std::ifstream		configFile;
		std::vector<Token>	tokens;
		std::vector<Server>	servers;

	public:
		Config();
		Config(const Config& other);
		Config& operator=(const Config& other);
		~Config();

		void	init(const char *file);

};

#endif