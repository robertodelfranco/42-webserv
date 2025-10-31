#ifndef LOCATIONCONFIG_HPP
# define LOCATIONCONFIG_HPP

#include "../Utils/structs.hpp"

struct LocationConfig {
	bool						autoindex; // caso tenha ou não autoindex ligado
	std::string					path; // caminho padrão do location
	std::string					root_override; // caso algo sobreponha o destino root
	std::vector<std::string>	allow_methods; // GET, POST e DELETE
};

#endif