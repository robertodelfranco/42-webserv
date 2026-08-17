#ifndef ERROR_RESPONSE_HPP
#define ERROR_RESPONSE_HPP

#include "Response.hpp"

class ServerConfig;
class Location;

class ErrorResponse : public Response {
	public:
		explicit ErrorResponse(int code,
							   const ServerConfig* server = NULL,
							   const Location* loc = NULL);

	private:
		static std::string	configuredPage(int code,
										   const ServerConfig* server,
										   const Location* loc);
		std::string			generatePage(int code) const;
};

#endif /* ERROR_RESPONSE_HPP */
