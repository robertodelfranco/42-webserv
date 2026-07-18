#ifndef LOCATION_HPP
# define LOCATION_HPP

#include <string>
#include <vector>
#include <map>
#include <cstddef>

enum Methods {
	GET = 1,
	POST = 2,
	DELETE = 4
};

class Location {
	public:
		std::string							path; // caminho padrão do location
		std::string							root; // caso algo sobreponha o destino root
		std::string							cgi_type; // caso tenha cgi
		std::string							cgi_path; // caminho para o executavel do cgi (pyhton3, Go e etc)
		std::vector<std::string>			index_files; // index definido no escopo do location
		std::map<int, std::string>			error_page; // páginas de erro definidas no escopo do location
		std::map<std::string, std::string>	redir; // caso tenha redirect de paginas
		bool								autoindex; // caso tenha ou não autoindex ligado
		size_t								allow_methods; // métodos permitidos "unificados" por bit (acesse por "&")
		long long							client_max_body_size; // caso tenha especificado dentro de location

		Location();

		void	setRoot(const std::string& root);
		void	setBodySize(long long size);
		void	setErrorPages(const std::vector<int>& error_pages, const std::string& path);
		void	setIndexFiles(const std::vector<std::string>& index_pages);
		void	setMethods(const std::vector<std::string>& methods);
		void	setRedirect(const std::string& code, const std::string& url);
		void	setCgi(const std::string& cgi_extension);
		void	setCgiPath(const std::string& cgi_path);
};

#endif
