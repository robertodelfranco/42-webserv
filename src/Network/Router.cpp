#include "Router.hpp"
#include "../Handlers/CgiHandler.hpp"
#include "../Utils/Logger.hpp"
#include "../Response/StaticHandler.hpp"
#include "../Response/ResponseHelpers.hpp"

Router::Router() {}

Router::~Router() {}

/* qual bloco Location do ServerConfig melhor se encaixa na URI */
const Location* Router::matchLocation(const ServerConfig& server, const std::string& uri)
{
	const Location*	best = NULL;
	size_t			bestLen = 0;

	for(std::vector<Location>::const_iterator it = server.getLocations().begin();
			it != server.getLocations().end(); it++) {
		size_t	len = it->getPath().size();

		bool	boundarys = (uri.size() == len) || (len < uri.size() && uri[len] == '/')
			|| (it->getPath() == "/");
		if (uri.compare(0, len, it->getPath()) == 0 && boundarys && len > bestLen) {
			bestLen = len;
			best = &(*it);
		}
	}
	if (best)
		Logger::debug() << "location casado: " << best->getPath() << " para " << uri;
	return best;
}

bool	Router::methodAllowed(const Location& loc, const HttpRequest& req) {
	size_t	bit = methodToBit(req.getMethod()); // tabela única, mora na Location.hpp

	return bit != 0 && (loc.getAllowMethods() & bit) != 0;
}

/* classifica se é CGI, static, dir, erro */
RouteType Router::classify(const Location& loc, const HttpRequest& req) {
	const std::string&	cgi_type = loc.getCgiType();
	const std::string&	path = req.getPath();

	if (!cgi_type.empty() && path.size() >= cgi_type.size()
			&& path.compare(path.size() - cgi_type.size(), cgi_type.size(), cgi_type) == 0)
		return CGI;

	return STATIC;
}

/* instancia e retorna a subclasse correta de IRequestHandler!
esse é o famoso design pattern chamado de factory method */
IRequestHandler* Router::createHandler(const Location& loc, const HttpRequest& req)
{
	switch (classify(loc, req)) {
		case CGI:
			return new CgiHandler();
		case STATIC:
			return new StaticHandler(req, loc);
		default:
			return NULL;
	}
}

// Delega pro resolveUriPath para existir UMA implementacao dessa regra no servidor inteiro
std::string	Router::resolvePath(const Location& loc, const std::string& uriPath) {
	return ResponseHelpers::resolveUriPath(loc, uriPath);
}
