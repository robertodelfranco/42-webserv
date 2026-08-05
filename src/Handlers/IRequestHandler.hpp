/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IRequestHandler.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eduribei <eduribei@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/25 13:04:32 by eduribei          #+#    #+#             */
/*   Updated: 2026/07/25 13:19:47 by eduribei         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef IREQUESTHANDLER_HPP
#define IREQUESTHANDLER_HPP

class HttpRequest;
class Location;
class Connection;

class IRequestHandler {
	public:
		/* destructor virtual para chamar o destructor certo de cada classe */
		virtual ~IRequestHandler() {}

		/*	handle recebe o request já parseado, apenas por referência const.
			o mesmo para location (const&), que será identificada pelo Router.
			
			já o connection é recebido sem const, porque será preciso modificar
			seu estado. mais especificamente, o CGI vai precisar completar
			informações sobre os fds de entrada e saída. se não fosse por isso,
			poderíamos passar apenas uma referencia a response e preencher seus
			dados. mas precisamos operar no escopo de Connection. em todos os
			casos, Response continua acessível, porém através de Connection. */
		virtual bool handle(const HttpRequest& req, 
							const Location& loc, 
							Connection& connection) = 0;
};

#endif
