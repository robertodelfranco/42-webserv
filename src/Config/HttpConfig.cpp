#include "HttpConfig.hpp"

HttpConfig::HttpConfig() : index(0), client_max_body__size(0) {}

HttpConfig::HttpConfig(const HttpConfig& other)
	: index(other.index), root(other.root), listens(other.listens),
	server_names(other.server_names), index_files(other.index_files),
	error_pages(other.error_pages), servers(other.servers),
	client_max_body__size(other.client_max_body__size) {}

HttpConfig&	HttpConfig::operator=(const HttpConfig& other) {
	if (this != &other) {
		index = other.index;
		root = other.root;
		listens = other.listens;
		server_names = other.server_names;
		index_files = other.index_files;
		error_pages = other.error_pages;
		servers = other.servers;
		client_max_body__size = other.client_max_body__size;
	}
	return *this;
}

HttpConfig::~HttpConfig() {}
