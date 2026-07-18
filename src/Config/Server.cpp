#include "Server.hpp"
#include <stdexcept>
#include <utility>
#include <cctype>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

Server::Server() : client_max_body_size(0) {}

Server::Server(const Server& other)
	: root(other.root), listens(other.listens), index_files(other.index_files),
	error_page(other.error_page), locations(other.locations),
	client_max_body_size(other.client_max_body_size) {}

Server& Server::operator=(const Server& other) {
	if (this != &other) {
		root = other.root;
		listens = other.listens;
		index_files = other.index_files;
		error_page = other.error_page;
		locations = other.locations;
		client_max_body_size = other.client_max_body_size;
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
	if (!S_ISDIR(info.st_mode))
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

	this->index_files.insert(this->index_files.end(), index_pages.begin(), index_pages.end());
}

void	Server::addLocation(const Location& location) {
	this->locations.push_back(location); // location já chega completo, montado localmente em getLocationBlock
}

const std::string&	Server::getRoot() const {
	return this->root;
}

const std::vector<Listen>&	Server::getListens() const {
	return this->listens;
}

const std::vector<std::string>&	Server::getIndexFiles() const {
	return this->index_files;
}

const std::map<int, std::string>&	Server::getErrorPages() const {
	return this->error_page;
}

const std::vector<Location>&	Server::getLocations() const {
	return this->locations;
}

long long	Server::getBodySize() const {
	return this->client_max_body_size;
}
