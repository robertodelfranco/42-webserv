/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eduribei <eduribei@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/26 11:15:30 by eduribei          #+#    #+#             */
/*   Updated: 2026/07/26 12:22:36 by eduribei         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Router.hpp"
#include "../Handlers/CgiHandler.hpp"
#include "../Utils/Logger.hpp"

Router::Router()
{
}

Router::~Router()
{
}

/* qual bloco Location do ServerConfig melhor se encaixa na URI */
const Location* Router::matchLocation(const ServerConfig& server, const std::string& uri)
{
	const Location*	best = NULL;
	size_t			bestLen = 0;

	for(std::vector<Location>::const_iterator it = server.getLocations().begin();
			it != server.getLocations().end(); it++) {
		size_t	len = it->getPath().size();
		bool	boundarys = (uri.size() == len) || (uri[len] == '/') || (it->getPath() == "/");
		if (uri.compare(0, len, it->getPath()) == 0 && boundarys && len > bestLen) {
			bestLen = len;
			best = &(*it);
		}
	}
	if (best)
		Logger::info() << "BEST " << best->getPath();
	return best;
}

/* bitmask usado aqui, 0 = método desconhecido */
static size_t	methodBit(const std::string& method) {
	if (method == "GET")
		return GET;
	if (method == "POST")
		return POST;
	if (method == "DELETE")
		return DELETE;
	return 0;
}

bool	Router::methodAllowed(const Location& loc, const HttpRequest& req) {
	size_t	bit = methodBit(req.getMethod());

	return bit != 0 && (loc.getAllowMethods() & bit) != 0;
}

/* classifica se é CGI, static, dir, erro */
RouteType Router::classify(const Location& loc, const HttpRequest& req) {
	const std::string&	cgi_type = loc.getCgiType();
	const std::string&	path = req.getPath();

	Logger::info() << "CLASSIFY " << path << " " << cgi_type;
	if (!cgi_type.empty() && path.size() >= cgi_type.size()
			&& path.compare(path.size() - cgi_type.size(), cgi_type.size(), cgi_type) == 0)
		return CGI;

	// por enquanto cai no 500 até ter o static handler, upload e etc.
	return ERROR;
}

/* instancia e retorna a subclasse correta de IRequestHandler!
esse é o famoso design pattern chamado de factory method */
IRequestHandler* Router::createHandler(const Location& loc, const HttpRequest& req)
{
	switch (classify(loc, req)) {
		case CGI:
			Logger::info() << "CGI";
			return new CgiHandler();
		
		default:
			return NULL;
	}
	return NULL;
}

std::string	Router::resolvePath(const Location& loc, const std::string& uriPath) {
	std::string	locRoot = loc.getRoot();
	
	// criar helpers startswith and endswith depois
	if (locRoot.compare(locRoot.size() - 1, 1, "/") == 0 && uriPath.compare(0, 1, "/") == 0)
		locRoot.append(uriPath.substr(1, uriPath.size()));
	else if (locRoot.compare(locRoot.size() - 1, 1, "/") == 0 || uriPath.compare(0, 1, "/") == 0)
		locRoot.append(uriPath);
	else
		locRoot.append("/").append(uriPath);
	
	return locRoot;
}
