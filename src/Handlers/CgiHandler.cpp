#include "CgiHandler.hpp"
#include "../Network/Router.hpp"
#include "../Network/Connection.hpp"
#include "../Utils/Logger.hpp"
#include <sstream>

bool	CgiHandler::handle(const HttpRequest& req, const Location& loc, Connection& conn) {
	Logger::info() << "CgiHandler: " << Router::resolvePath(loc, req.getPath());

	std::ostringstream	oss;
	std::string			body = "501 Not Implemented\n"; 

	oss << "HTTP/1.1 501 Not Implemented\r\n"
		<< "Content-Type: text/plain\r\n"
		<< "Content-length: " << body.size() << "\r\n"
		<< "Connection: " << (conn.wantsKeepAlive() ? "keep-alive" : "close") << "\r\n"
		<< "\r\n"
		<< body;

	conn.queueResponse(oss.str());
	return true;
}
