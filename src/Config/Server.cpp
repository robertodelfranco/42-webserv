#include "Server.hpp"

Server::Server() : index(0), client_max_body_size(0) {}

Server::Server(const Server& other)
	: index(other.index), root(other.root), listens(other.listens),
	server_names(other.server_names), index_files(other.index_files),
	error_pages(other.error_pages), locations(other.locations),
	client_max_body_size(other.client_max_body_size) {}

Server& Server::operator=(const Server& other) {
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
}

Server::~Server() {}
