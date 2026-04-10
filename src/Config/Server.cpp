#include "Server.hpp"

Server::Server() : client_max_body_size(0), current_location(NULL) {}

Server::Server(const Server& other)
	: root(other.root), listens(other.listens), index_files(other.index_files),
	error_page(other.error_page), locations(other.locations), 
	client_max_body_size(other.client_max_body_size), current_location(other.current_location) {}

Server& Server::operator=(const Server& other) {
	if (this != &other) {
		root = other.root;
		listens = other.listens;
		index_files = other.index_files;
		error_page = other.error_page;
		locations = other.locations;
		client_max_body_size = other.client_max_body_size;
		current_location = other.current_location;
	}
	return *this;
}

Server::~Server() {}

void	Server::setListen(const std::string& host, const std::string& port) {
	if (port.empty())
		throw std::runtime_error("Listen: port is required");

	for (size_t i = 0; i < port.size(); ++i) {
		if (!std::isdigit(port[i]))
			throw std::runtime_error("Listen: invalid port '" + port + "'");
	}

	int port_num = std::atoi(port.c_str());

	if (port_num < 1 || port_num > 65535)
		throw std::runtime_error("Listen: port out of range (1-65535)");

	std::string resolved_host = host.empty() ? "0.0.0.0" : host;

	for (size_t i = 0; i < listens.size(); ++i) {
		if (listens[i].host == resolved_host && listens[i].port == port_num)
			throw std::runtime_error("Listen: duplicate listen " + resolved_host + ":" + port);
	}

	listens.push_back(Listen(resolved_host, port_num));
}

void	Server::setRoot(const std::string& root) {
	if (root.empty())
		throw std::runtime_error("Root: path is required");
	if (root[0] != '/' && root[0] != '.')
		throw std::runtime_error("Root: path must be absolute or relative to config file '" + root + "'");
	
	struct stat info;
	if (stat(root.c_str(), &info) != 0)
		throw std::runtime_error("Root: path does not exist '" + root + "'");
	if (!(info.st_mode & S_IFDIR))
		throw std::runtime_error("Root: path is not a directory '" + root + "'");
	if (access(root.c_str(), R_OK) != 0)
		throw std::runtime_error("Root: no read permission on '" + root + "'");

	this->root = root;
}

void	Server::setBodySize(long long size) {
	if (size < 0)
		throw std::runtime_error("Body size cannot be negative");
	this->client_max_body_size = size;
}

void	Server::setErrorPages(const std::vector<int>& error_pages, const std::string& path) {
	if (error_pages.empty())
		throw std::runtime_error("Error page: at least one error code is required");

	if (path.empty())
		throw std::runtime_error("Error page: path is required");
	
	if (path[0] != '/' && path[0] != '.')
		throw std::runtime_error("Error page: path must be absolute or relative to config file '" + path + "'");
	
	for (size_t i = 0; i < error_pages.size(); ++i) {
		this->error_page.insert(std::make_pair(error_pages[i], path));
	}
}

void	Server::setIndexFiles(const std::vector<std::string>& index_pages) {
	if (index_pages.empty())
		throw std::runtime_error("Index: at least one index file is required");

	// eu criei um ponteiro para o vetor de index_files do server ou location dependendo do contexto, para evitar if else e código duplicado, depois eu insiro nesse ponteiro que aponta pra variavel do contexto e preencho ela
	std::vector<std::string>& target = (this->current_location == NULL)
		? this->index_files 
		: this->current_location->index_files;

	target.insert(target.end(), index_pages.begin(), index_pages.end());
}

void	Server::setMethods(const std::vector<std::string>& methods) {
	if (methods.empty())
		throw std::runtime_error("Methods: at least one method is required");
	
	size_t allow_methods = 0;
	for (size_t i = 0; i < methods.size(); ++i) {
		std::string method = methods[i]; // create to upper function later
		if (method == "GET")
			allow_methods |= (1 << 0);
		else if (method == "POST")
			allow_methods |= (1 << 1);
		else if (method == "DELETE")
			allow_methods |= (1 << 2);
		else
			throw std::runtime_error("Methods: invalid method '" + methods[i] + "'");
	}

	if (this->current_location == NULL)
		this->client_max_body_size = allow_methods;
	else
		this->current_location->allow_methods = allow_methods;
}

void	Server::setRedirect(const std::string& code, const std::string& url) {
	if (this->current_location == NULL)
		throw std::runtime_error("Redirect: return directive is only allowed in location context");
	
	this->current_location->redir.insert(std::make_pair(code, url));	
	
}

void	Server::setCgi(const std::string& cgi_extension) {
	if (this->current_location == NULL)
		throw std::runtime_error("CGI: cgi_type directive is only allowed in location context");
	
	this->current_location->cgi_type = cgi_extension;
}

void	Server::setCgiPath(const std::string& cgi_path) {
	if (this->current_location == NULL)
		throw std::runtime_error("CGI path: cgi_path directive is only allowed in location context");
	
	this->current_location->cgi_path = cgi_path;
}

void	Server::setLocation(const Location& location) {
	this->current_location = const_cast<Location*>(&location); // seto o ponteiro para o location passado, para as próximas diretivas de location setarem os dados nesse location
	this->locations.push_back(location); // adiciona o location no vetor de locations do server
}

void	Server::unsetLocation() {
	this->current_location = NULL; // desseta o ponteiro para evitar que diretivas fora do location setem dados no location por engano
}
