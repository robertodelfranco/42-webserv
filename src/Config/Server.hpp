#ifndef SERVER_HPP
#define SERVER_HPP

#include "../Utils/structs.hpp"
#include "Listen.hpp"
#include "LocationConfig.hpp"

class Server {
	private:
		size_t	index; // index dos arquivos servers recebidos por parâmetro
		std::vector<Listen> listens; // portas abertas
		std::vector<std::string> server_name; // host name
		std::string	root; // caminho root definido
		std::vector<std::string> index_files; // arquivos passados pelo curl
		std::map<int, std::string> error_pages; // páginas de erros definidas no config
		std::map<std::string, LocationConfig> locations; // métodos permitidos, autoindex e etc
		long long	client_max_body_size; // content-length máximo do body da request

	public:
		Server() {};
		Server(const Server& other);
		Server& operator=(const Server& other);
		~Server() {};
};

#endif