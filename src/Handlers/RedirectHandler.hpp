#ifndef REDIRECTHANDLER_HPP
# define REDIRECTHANDLER_HPP

#include "IRequestHandler.hpp"

/* Um location com "return <3xx> <url>" não serve conteúdo nenhum, só
aponta pra outro lugar. Não há disco nem processo filho envolvido, então
o handler monta a resposta na hora e devolve true (acabei). */
class RedirectHandler : public IRequestHandler {
	public:
		virtual bool	handle(const HttpRequest& req, const Location& loc, Connection& conn);
};

#endif
