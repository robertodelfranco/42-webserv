#ifndef CGIHANDLER_HPP
# define CGIHANDLER_HPP

#include <string>
#include <vector>
#include "IRequestHandler.hpp"

class CgiProcess;

/* Ele valida, monta o ambiente, lança o filho, entrega o
CgiProcess pro Connection e morre retornando false, que é o
jeito de dizer "ainda não acabei, me acorda pelo poll()". */
class CgiHandler : public IRequestHandler {
	public:
		virtual bool	handle(const HttpRequest& req, const Location& loc, Connection& conn);

	private:
		static std::vector<std::string>	buildEnv(const HttpRequest& req,
												 Connection& conn,
												 const std::string& scriptPath);
};

#endif
