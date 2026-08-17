#include "Location.hpp"
#include "ServerConfig.hpp"
#include "../Response/ResponseHelpers.hpp"
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

size_t	methodToBit(const std::string& method) {
	if (method == "GET")
		return GET;
	if (method == "POST")
		return POST;
	if (method == "DELETE")
		return DELETE;
	if (method == "HEAD")
		return HEAD;
	return 0;
}

Location::Location() : path_(""), root_(""), cgi_type_(""),
	cgi_path_(""), index_files_(), error_page_(), redir_(),
	autoindex_(false), allow_methods_(GET), client_max_body_size_(-1),
	upload_path_("") {} // allow_methods começa só com GET -- se ninguém declarar "methods", o location fica só de leitura em vez de bloquear tudo (0) ou liberar POST/DELETE sem ninguém ter pedido

void	Location::setPath(const std::string& path) {
	if (path.empty())
		throw std::runtime_error("Location: path is required");

	if (path[0] != '/')
		throw std::runtime_error("Location: path must start with '/' '" + path + "'");

	if (path != "/" && path[path.size() - 1] == '/') {
		this->path_ = path.substr(0, path.size() - 1);
		return ;
	}

	this->path_ = path;
}

void	Location::setRoot(const std::string& root) {
	if (root.empty())
		throw std::runtime_error("Root: path is required");

	if (root[0] != '/' && root[0] != '.')
		throw std::runtime_error("Root: path must start with '/' (absolute) or './' (relative) '" + root + "'");

	struct stat info;
	if (stat(root.c_str(), &info) != 0)
		throw std::runtime_error("Root: path does not exist '" + root + "'");

	if (!S_ISDIR(info.st_mode))
		throw std::runtime_error("Root: path is not a directory '" + root + "'");

	if (access(root.c_str(), R_OK) != 0)
		throw std::runtime_error("Root: no read permission on '" + root + "'");

	this->root_ = root;
}

void	Location::setBodySize(long long size) {
	if (size < 0)
		throw std::runtime_error("Body size cannot be negative");

	this->client_max_body_size_ = size;
}

void	Location::setErrorPages(const std::vector<int>& error_pages, const std::string& path) {
	if (error_pages.empty())
		throw std::runtime_error("Error page: at least one error code is required");

	if (path.empty())
		throw std::runtime_error("Error page: path is required");

	if (path[0] != '/' && path[0] != '.')
		throw std::runtime_error("Error page: path must start with '/' (absolute) or './' (relative) '" + path + "'");

	for (size_t i = 0; i < error_pages.size(); ++i)
		this->error_page_.insert(std::make_pair(error_pages[i], path));
}

void	Location::setIndexFiles(const std::vector<std::string>& index_pages) {
	if (index_pages.empty())
		throw std::runtime_error("Index: at least one index file is required");

	this->index_files_.insert(this->index_files_.end(), index_pages.begin(), index_pages.end());
}

void	Location::setMethods(const std::vector<std::string>& methods) {
	if (methods.empty())
		throw std::runtime_error("Methods: at least one method is required");

	size_t allow_methods = 0;
	for (size_t i = 0; i < methods.size(); ++i) {
		size_t	bit = methodToBit(methods[i]); // create 'to upper' function later
		if (bit == 0)
			throw std::runtime_error("Methods: invalid method '" + methods[i] + "'");
		allow_methods |= bit;
	}

	this->allow_methods_ = allow_methods;
}

void	Location::setRedirect(const std::string& code, const std::string& url) {
	this->redir_.insert(std::make_pair(code, url));
}

void	Location::setCgi(const std::string& cgi_extension) {
	this->cgi_type_ = cgi_extension;
}

void	Location::setCgiPath(const std::string& cgi_path) {
	this->cgi_path_ = cgi_path;
}

void	Location::setAutoindex(bool value) {
	this->autoindex_ = value;
}

void	Location::setUploadPath(const std::string& path) {
	if (path.empty())
		throw std::runtime_error("Upload path: path is required");

	if (path[0] != '/' && path[0] != '.')
		throw std::runtime_error("Upload path: path must start with '/' (absolute) or './' (relative) '" + path + "'");

	struct stat info;
	if (stat(path.c_str(), &info) != 0)
		throw std::runtime_error("Upload path: path does not exist '" + path + "'");

	if (!S_ISDIR(info.st_mode))
		throw std::runtime_error("Upload path: path is not a directory '" + path + "'");

	if (access(path.c_str(), W_OK) != 0)
		throw std::runtime_error("Upload path: no write permission on '" + path + "'");

	this->upload_path_ = path;
}

void	Location::mergeDefaults(const ServerConfig& server) {
	if (this->root_.empty())
		this->root_ = ResponseHelpers::joinPath(server.getRoot(), path_);

	if (this->index_files_.empty())
		this->index_files_ = server.getIndexFiles();

	if (this->error_page_.empty())
		this->error_page_ = server.getErrorPages();

	if (this->client_max_body_size_ < 0)
		this->client_max_body_size_ = server.getBodySize();

}

const std::string&	Location::getPath() const {
	return path_;
}

const std::string&	Location::getRoot() const {
	return root_;
}

const std::string&	Location::getCgiType() const {
	return cgi_type_;
}

const std::string&	Location::getCgiPath() const {
	return cgi_path_;
}

const std::vector<std::string>&	Location::getIndexFiles() const {
	return index_files_;
}

const std::map<int, std::string>& Location::getErrorPages() const {
	return error_page_;
}

const std::map<std::string, std::string>& Location::getRedir() const {
	return redir_;
}

bool Location::getAutoIndex() const {
	return autoindex_;
}

size_t Location::getAllowMethods() const {
	return allow_methods_;
}

long long Location::getClientMaxBodySize() const {
	return client_max_body_size_;
}

const std::string& Location::getUploadPath() const {
	return upload_path_;
}
