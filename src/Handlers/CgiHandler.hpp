#ifndef CGIHANDLER_HPP
# define CGIHANDLER_HPP

#include <string>
#include <vector>
#include "IRequestHandler.hpp"

class CgiProcess;

/* Primeira subclasse concreta de IRequestHandler, e a mais estranha das
quatro: é a única que NÃO tem resposta pronta quando o handle() acaba.

O Connection deleta o handler assim que o handle() retorna, então este objeto
não pode supervisionar processo nenhum. Ele valida, monta o ambiente, lança o
filho, entrega o CgiProcess pro Connection e morre - retornando false, que é o
jeito de dizer "ainda não acabei, me acorda pelo poll()". */
class CgiHandler : public IRequestHandler {
	public:
		virtual bool	handle(const HttpRequest& req, const Location& loc, Connection& conn);

	private:
		/* As meta-variáveis do CGI/1.1. Devolve por valor um vector que o
		CgiProcess guarda como membro: as strings precisam continuar vivas até
		o execve, e o envp é só um punhado de ponteiros pra dentro delas. */
		static std::vector<std::string>	buildEnv(const HttpRequest& req,
												 Connection& conn,
												 const std::string& scriptPath);
};

#endif
