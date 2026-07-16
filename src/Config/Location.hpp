#ifndef LOCATION_HPP
# define LOCATION_HPP

#include "../Utils/Utils.hpp"

class Location {
	public:
		std::string							path; // caminho padrão do location
		std::string							root; // caso algo sobreponha o destino root
		std::string							cgi_type; // caso tenha cgi
		std::string							cgi_path; // caminho para o executavel do cgi (pyhton3, Go e etc)
		std::vector<std::string>			index_files; // index definido no escopo do location
		std::map<std::string, std::string>	redir; // caso tenha redirect de paginas			
		bool								autoindex; // caso tenha ou não autoindex ligado
		size_t								allow_methods; // métodos permitidos "unificados" por bit (acesse por "&")
		long long							client_max_body_size; // caso tenha especificado dentro de location

		Location();

		void	setMethods(const std::vector<std::string>& methods);
};

#endif