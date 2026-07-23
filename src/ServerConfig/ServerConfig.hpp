#ifndef SERVERCONFIG_HPP
# define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include "Listen.hpp"
#include "Location.hpp"

// Dados de UM bloco server{} do arquivo de config, não é "o servidor
// rodando" (isso é EventLoop/Socket/Connection, em Network/), é só o
// resultado parseado que essas classes vão consultar pra saber root,
// listens, locations e de cada request.
class ServerConfig {
	private:
		std::string						root; // caminho root definido
		std::vector<Listen>				listens; // portas abertas
		std::vector<std::string>		index_files; // arquivos html
		std::map<int, std::string>		error_page; // páginas de erros definidas no config
		std::vector<Location>			locations; // cada bloco location dentro de server
		long long						client_max_body_size; // content-length máximo do body da request

	public:
		ServerConfig();
		ServerConfig(const ServerConfig& other);
		ServerConfig& operator=(const ServerConfig& other);
		~ServerConfig();

		void	setListen(const std::string& host, const std::string& port);
		void	setRoot(const std::string& root);
		void	setBodySize(long long size);
		void	setErrorPages(const std::vector<int>& error_pages, const std::string& path);
		void	setIndexFiles(const std::vector<std::string>& index_pages);
		void	setServerNames(const std::vector<std::string>& names);
		void	addLocation(const Location& location); // adiciona um location já completo no vetor de locations

		const std::string&					getRoot() const;
		const std::vector<Listen>&			getListens() const;
		const std::vector<std::string>&	getServerNames() const;
		const std::vector<std::string>&	getIndexFiles() const;
		const std::map<int, std::string>&	getErrorPages() const;
		const std::vector<Location>&		getLocations() const;
		long long							getBodySize() const;
};

#endif
