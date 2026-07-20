#ifndef RESPONSE_HANDLE_HPP
#define RESPONSE_HANDLE_HPP

#include "../Utils/structs.hpp"

class ResponseBuilder {
	private:
		const HTTPRequest &request; //type of request GET/DELETE/POST
		const Location &location; //server route

		Response handleGET();
		Response handlePOST();
		Response handleDELETE();
		Response handleHEAD();
		Response handleMethodAllowed();
		Response generateAutoindex(const std::string &dir_path);

		bool isMethodAllowed(const std::string &method) const;
		Methods methodToEnum(const std::string &method) const;
		std::string getMimeType(const std::string &path) const;


	public:
		ResponseBuilder(const HTTPRequest &req, const Location &loc);
        Response build();
};

#endif /* RESPONSE_HANDLE_HPP */