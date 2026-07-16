#include "Location.hpp"

Location::Location() : path(""), root(""), cgi_type(""), cgi_path(""), index_files(), redir(), autoindex(false), allow_methods(0), client_max_body_size(0) {}

void	Location::setMethods(const std::vector<std::string>& methods) {
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

	this->allow_methods = allow_methods;
}
