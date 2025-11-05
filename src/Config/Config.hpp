#ifndef CONFIG_HPP
# define CONFIG_HPP

#include "../Utils/structs.hpp"
#include "Server.hpp"

class Config {
	private:
		std::vector<Server>	servers;

	public:
		Config();
		Config(const Config& other);
		Config& operator=(const Config& other);
		~Config();

		void	init(const char *file);
};

#endif