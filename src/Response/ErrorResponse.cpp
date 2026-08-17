#include "ErrorResponse.hpp"
#include <sstream>
#include "ResponseHelpers.hpp"
#include "../ServerConfig/ServerConfig.hpp"
#include "../ServerConfig/Location.hpp"

std::string	ErrorResponse::configuredPage(int code, const ServerConfig* server,
										  const Location* loc) {
	if (loc) {
		const std::map<int, std::string>&			pages = loc->getErrorPages();
		std::map<int, std::string>::const_iterator	it = pages.find(code);

		if (it != pages.end())
			return ResponseHelpers::joinPath(loc->getRoot(), it->second);
	}

	if (server) {
		const std::map<int, std::string>&			pages = server->getErrorPages();
		std::map<int, std::string>::const_iterator	it = pages.find(code);

		if (it != pages.end())
			return ResponseHelpers::joinPath(server->getRoot(), it->second);
	}

	return std::string();
}

std::string	ErrorResponse::generatePage(int code) const {
	std::ostringstream	ss;
	const std::string	phrase = statusMessageCode(code);

	ss	<< "<html><head><title>" << code << " " << phrase << "</title></head>"
		<< "<body><h1>" << code << " " << phrase << "</h1>"
		<< "<hr><p>webserv</p></body></html>";
	return ss.str();
}

ErrorResponse::ErrorResponse(int code, const ServerConfig* server, const Location* loc) {
	const std::string	page = configuredPage(code, server, loc);
	std::string			body;

	setStatus(code);

	// se a pagina configurada nao abrir, gerar uma e melhor do que
	// responder outro erro por causa do erro original
	if (page.empty() || !ResponseHelpers::readFileToString(page, body))
		body = generatePage(code);

	setHeader("Content-Type", "text/html");
	setBody(body);
	setCloseAfterSend(true);
}
