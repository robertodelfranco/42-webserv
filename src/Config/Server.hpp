#ifndef SERVER_HPP
#define SERVER_HPP

#include "../Utils/structs.hpp"
#include "Listen.hpp"
#include "Location.hpp"

class Server {
	private:
		size_t							index; // index dos arquivos servers recebidos por parâmetro
		std::string						root; // caminho root definido
		std::vector<Listen>				listens; // portas abertas
		std::vector<std::string>		server_names; // host names
		std::vector<std::string>		index_files; // arquivos passados pelo curl
		std::map<int, std::string>		error_pages; // páginas de erros definidas no config
		std::map<std::string, Location>	locations; // métodos permitidos, autoindex e etc
		long long						client_max_body_size; // content-length máximo do body da request

	public:
		Server() : index(0), client_max_body_size(0) {};
		Server(const Server& other) : index(other.index), root(other.root), listens(other.listens), server_names(other.server_names), index_files(other.index_files), error_pages(other.error_pages), locations(other.locations), client_max_body_size(other.client_max_body_size) {};
		Server& operator=(const Server& other) {
			if (this != &other) {
				index = other.index;
				root = other.root;
				listens = other.listens;
				server_names = other.server_names;
				index_files = other.index_files;
				error_pages = other.error_pages;
				locations = other.locations;
				client_max_body_size = other.client_max_body_size;
			}
			return *this;
		};
		~Server() {};
};

#endif