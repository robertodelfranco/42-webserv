#include "Location.hpp"
#include "ServerConfig.hpp"
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

Location::Location() : path(""), root(""), cgi_type(""), cgi_path(""), index_files(), error_page(), redir(), autoindex(false), allow_methods(GET), client_max_body_size(-1), upload_path("") {} // allow_methods começa só com GET -- se ninguém declarar "methods", o location fica só de leitura em vez de bloquear tudo (0) ou liberar POST/DELETE sem ninguém ter pedido

void	Location::setRoot(const std::string& root) {
	if (root.empty())
		throw std::runtime_error("Root: path is required");
	if (root[0] != '/' && root[0] != '.')
		throw std::runtime_error("Root: path must be absolute or relative to config file '" + root + "'");

	struct stat info;
	if (stat(root.c_str(), &info) != 0)
		throw std::runtime_error("Root: path does not exist '" + root + "'");
	if (!S_ISDIR(info.st_mode))
		throw std::runtime_error("Root: path is not a directory '" + root + "'");
	if (access(root.c_str(), R_OK) != 0)
		throw std::runtime_error("Root: no read permission on '" + root + "'");

	this->root = root;
}

void	Location::setBodySize(long long size) {
	if (size < 0)
		throw std::runtime_error("Body size cannot be negative");
	this->client_max_body_size = size;
}

void	Location::setErrorPages(const std::vector<int>& error_pages, const std::string& path) {
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

void	Location::setIndexFiles(const std::vector<std::string>& index_pages) {
	if (index_pages.empty())
		throw std::runtime_error("Index: at least one index file is required");

	this->index_files.insert(this->index_files.end(), index_pages.begin(), index_pages.end());
}

void	Location::setMethods(const std::vector<std::string>& methods) {
	if (methods.empty())
		throw std::runtime_error("Methods: at least one method is required");

	size_t allow_methods = 0;
	for (size_t i = 0; i < methods.size(); ++i) {
		std::string method = methods[i]; // create 'to upper' function later
		if (method == "GET")
			allow_methods |= GET;
		else if (method == "POST")
			allow_methods |= POST;
		else if (method == "DELETE")
			allow_methods |= DELETE;
		else
			throw std::runtime_error("Methods: invalid method '" + methods[i] + "'");
	}

	this->allow_methods = allow_methods;
}

void	Location::setRedirect(const std::string& code, const std::string& url) {
	this->redir.insert(std::make_pair(code, url));
}

void	Location::setCgi(const std::string& cgi_extension) {
	this->cgi_type = cgi_extension;
}

void	Location::setCgiPath(const std::string& cgi_path) {
	this->cgi_path = cgi_path;
}

void	Location::setAutoindex(bool value) {
	this->autoindex = value;
}

void	Location::setUploadPath(const std::string& path) {
	if (path.empty())
		throw std::runtime_error("Upload path: path is required");
	if (path[0] != '/' && path[0] != '.')
		throw std::runtime_error("Upload path: path must be absolute or relative to config file '" + path + "'");

	struct stat info;
	if (stat(path.c_str(), &info) != 0)
		throw std::runtime_error("Upload path: path does not exist '" + path + "'");
	if (!S_ISDIR(info.st_mode))
		throw std::runtime_error("Upload path: path is not a directory '" + path + "'");
	if (access(path.c_str(), W_OK) != 0)
		throw std::runtime_error("Upload path: no write permission on '" + path + "'");

	this->upload_path = path;
}

void	Location::mergeDefaults(const ServerConfig& server) {
	if (this->root.empty())
		this->root = server.getRoot();
	if (this->index_files.empty())
		this->index_files = server.getIndexFiles();
	if (this->error_page.empty())
		this->error_page = server.getErrorPages();
	if (this->client_max_body_size < 0)
		this->client_max_body_size = server.getBodySize();
}
