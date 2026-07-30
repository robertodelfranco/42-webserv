#ifndef SERVER_HPP
# define SERVER_HPP

#include "../Utils/structs.hpp"

class Server {
	private:
		size_t							index; // index dos arquivos servers recebidos por parâmetro
		std::string						root; // caminho root definido
		std::vector<Listen>				listens; // portas abertas
		std::vector<std::string>		server_names; // host names
		std::vector<std::string>		index_files; // arquivos passados pelo curl
		std::map<int, std::string>		error_pages; // páginas de erros definidas no config
		std::vector<Location>			locations; // cada bloco location dentro de server
		long long						client_max_body__size; // content-length máximo do body_ da request

	public:
		Server();
		Server(const Server& other);
		Server& operator=(const Server& other);
		~Server();
};

#endif