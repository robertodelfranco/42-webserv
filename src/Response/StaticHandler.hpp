#ifndef STATIC_HANDLER_HPP
#define STATIC_HANDLER_HPP

#include "../Request/HttpRequest.hpp"
#include "../ServerConfig/Location.hpp"
#include "Response.hpp"
#include "ErrorResponse.hpp"
#include "ResponseHelpers.hpp"

class StaticHandler {
	private:
		const HttpRequest &request; //type of request GET/DELETE/POST
		const Location &location; //server route

		Response handleGET();
		Response handlePOST();
		Response handleDELETE();
		Response handleHEAD();
		Response handleMethodAllowed();

		bool isMethodAllowed(const std::string &method) const;
		Methods methodToEnum(const std::string &method) const;


	public:
		StaticHandler(const HttpRequest &req, const Location &loc);
        Response build();
};

#endif /* STATIC_HANDLER_HPP */