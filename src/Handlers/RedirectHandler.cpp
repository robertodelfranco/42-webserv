#include "RedirectHandler.hpp"
#include "../Network/Connection.hpp"
#include "../Response/Response.hpp"
#include "../ServerConfig/Location.hpp"
#include "../Utils/Logger.hpp"
#include <cstdlib>

bool	RedirectHandler::handle(const HttpRequest& req, const Location& loc, Connection& conn) {
	(void)req; // o redirect independe de método, path ou body

	const std::map<std::string, std::string>&			redir = loc.getRedir();
	std::map<std::string, std::string>::const_iterator	it = redir.begin();

	if (it == redir.end()) {
		Logger::error() << "fd=" << conn.getFd() << " RedirectHandler sem 'return' em "
			<< loc.getPath();
		conn.buildErrorResponse(500);
		return true;
	}

	Response	resp;

	resp.setStatus(std::atoi(it->first.c_str()));
	resp.setHeader("Location", it->second);
	resp.setBody("");

	Logger::info() << "fd=" << conn.getFd() << " redirect " << it->first
		<< " -> " << it->second;

	conn.sendResponse(resp);
	return true;
}
