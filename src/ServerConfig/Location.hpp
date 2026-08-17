#ifndef LOCATION_HPP
# define LOCATION_HPP

#include <string>
#include <vector>
#include <map>
#include <cstddef>

enum Methods {
	GET = 1,
	POST = 2,
	DELETE = 4,
	HEAD = 8
};

/* Mora aqui pra existir uma tabela dessas no servidor inteiro, config,
router e handlers precisam concordar sobre o que cada bit vale */
size_t	methodToBit(const std::string& method);

class ServerConfig;

class Location {
	private:
		std::string							path_; // caminho padrão do location
		std::string							root_; // caso algo sobreponha o destino root
		std::string							cgi_type_; // caso tenha cgi
		std::string							cgi_path_; // caminho para o executavel do cgi (pyhton3, Go e etc)
		std::vector<std::string>			index_files_; // index definido no escopo do location
		std::map<int, std::string>			error_page_; // páginas de erro definidas no escopo do location
		std::map<std::string, std::string>	redir_; // caso tenha redirect de paginas
		bool								autoindex_; // caso tenha ou não autoindex ligado
		size_t								allow_methods_; // métodos permitidos "unificados" por bit (acesse por "&")
		long long							client_max_body_size_; // caso tenha especificado dentro de location
		std::string							upload_path_; // diretório onde uploads (POST) desse location são salvos
	
	public:
		Location();

		void	setPath(const std::string& path);
		void	setRoot(const std::string& root);
		void	setBodySize(long long size);
		void	setErrorPages(const std::vector<int>& error_pages, const std::string& path);
		void	setIndexFiles(const std::vector<std::string>& index_pages);
		void	setMethods(const std::vector<std::string>& methods);
		void	setRedirect(const std::string& code, const std::string& url);
		void	setCgi(const std::string& cgi_extension);
		void	setCgiPath(const std::string& cgi_path);
		void	setAutoindex(bool value);
		void	setUploadPath(const std::string& path);

		void	mergeDefaults(const ServerConfig& server); // preenche root/index/error_page/client_max_body_size com o valor do server, quando o location não declarou o próprio

		const std::string&							getPath() const;
		const std::string& 							getRoot() const;
		const std::string& 							getCgiType() const;
		const std::string& 							getCgiPath() const;
		const std::vector<std::string>&				getIndexFiles() const;
		const std::map<int, std::string>&			getErrorPages() const;
		const std::map<std::string, std::string>&	getRedir() const;
		bool										getAutoIndex() const;
		size_t										getAllowMethods() const;
		long long									getClientMaxBodySize() const;
		const std::string&							getUploadPath() const;
};

#endif
