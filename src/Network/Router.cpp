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

Router::Router()
{
}

Router::~Router()
{
}

/* qual bloco Location do ServerConfig melhor se encaixa na URI */
const Location* Router::matchLocation(const ServerConfig& server,
											 const std::string& uri)
{
	(void)server;
	(void)uri;
	return NULL; //TODO	
}

/* classifica se é CGI, static, dir, erro */									 
RouteType Router::classify(const Location& loc, const HttpRequest& req)
{
	(void)loc;
	(void)req;
	return ERROR; //TODO
}

/* instancia e retorna a subclasse correta de IRequestHandler!
esse é o famoso design pattern chamado de factory method */
IRequestHandler* Router::createHandler(const Location& loc,
											  const HttpRequest& req)
{
	(void)loc;
	(void)req;
	return NULL; //TODO
}