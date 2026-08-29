#ifndef ROUTER_HPP
# define ROUTER_HPP

# include <string>
#include "../ServerConfig/ServerConfig.hpp"
#include "../ServerConfig/Location.hpp"
#include "../Request/HttpRequest.hpp"
#include "../Handlers/IRequestHandler.hpp"

/* enum que representa o tipo de rota pedida pela URI */
enum RouteType
{
	STATIC,
	CGI,
	REDIRECT,
	DIR,
	ERROR
};

class Router
{
	private:
		/* os construtores ficam privados pq Router é uma classe stateless.
		ninguém vai instanciar "Router router". repara que aí embaixo que tem
		um "static" antes de cada método, sem isso não dá para chamar o método
		sem instaciar a classe.*/
		Router();
		Router(const Router& other);
		Router& operator=(const Router& other);
		~Router();

	public:
		/* qual bloco Location do ServerConfig melhor se encaixa na URI */
		static const Location* matchLocation(const ServerConfig& server,
											 const std::string& uri);

		/* classifica se é CGI, static, dir, erro */
		static RouteType classify(const Location& loc, const HttpRequest& req);

		/* o método da request está no "methods" deste location? A checagem é
		do Connection porque a resposta é 405, não um handler diferente. */
		static bool methodAllowed(const Location& loc, const HttpRequest& req);

		/* instancia e retorna a subclasse correta de IRequestHandler!
		esse é o famoso design pattern chamado de factory method */
		static IRequestHandler* createHandler(const Location& loc,
											  const HttpRequest& req);

		static std::string resolvePath(const Location& loc, const std::string& uriPath);
};

#endif
