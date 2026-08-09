#ifndef CGIHANDLER_HPP
# define CGIHANDLER_HPP

#include <iostream>
#include "IRequestHandler.hpp"

class CgiHandler : public IRequestHandler {
	public:
		virtual bool	handle(const HttpRequest& req, const Location& loc, Connection& conn);
};

#endif