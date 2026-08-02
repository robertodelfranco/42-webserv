#ifndef HTTPCONFIG_HPP
# define HTTPCONFIG_HPP

#include "../Utils/structs.hpp"
#include "Server.hpp"

class HttpConfig {
	private:
		size_t							index; // index dos arquivos servers recebidos por parâmetro
		std::string						root; // caminho root definido
		std::vector<Listen>				listens; // portas abertas
		std::vector<std::string>		server_names; // host names
		std::vector<std::string>		index_files; // arquivos passados pelo curl
		std::map<int, std::string>		error_pages; // páginas de erros definidas no config
		std::vector<Server>				servers; // blocos de servidores
		long long						client_max_body__size; // content-length máximo do body_ da request

	public:
		HttpConfig();
		HttpConfig(const HttpConfig& other);
		HttpConfig& operator=(const HttpConfig& other);
		~HttpConfig();
};

#endif