#ifndef SERVER_HPP
# define SERVER_HPP

#include "../Utils/structs.hpp"

class Server {
	private:
		std::string						root; // caminho root definido
		std::vector<Listen>				listens; // portas abertas
		// std::vector<std::string>		host; // host ip (ja tem dentro do listen)
		std::vector<std::string>		index_files; // arquivos html
		std::map<int, std::string>		error_page; // páginas de erros definidas no config
		std::vector<Location>			locations; // cada bloco location dentro de server
		long long						client_max_body_size; // content-length máximo do body da request

	public:
		Server();
		Server(const Server& other);
		Server& operator=(const Server& other);
		~Server();

		void	setListen(const std::string& host, const std::string& port);
		void	setRoot(const std::string& root);
		void	setBodySize(long long size);
		void	setErrorPages(const std::vector<int>& error_pages, const std::string& path);
};

#endif