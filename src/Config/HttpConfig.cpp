#include "HttpConfig.hpp"

HttpConfig::HttpConfig() : index(0), client_max_body_size(0) {}

HttpConfig::HttpConfig(const HttpConfig& other)
	: index(other.index), root(other.root), listens(other.listens),
	server_names(other.server_names), index_files(other.index_files),
	error_page(other.error_page),
	client_max_body_size(other.client_max_body_size) {}

HttpConfig&	HttpConfig::operator=(const HttpConfig& other) {
	if (this != &other) {
		index = other.index;
		root = other.root;
		listens = other.listens;
		server_names = other.server_names;
		index_files = other.index_files;
		error_page = other.error_page;
		client_max_body_size = other.client_max_body_size;
	}
	return *this;
}

HttpConfig::~HttpConfig() {}
