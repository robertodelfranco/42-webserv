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

/*	Os erros semânticos (403, 404, 405, 502, 504) vêm de uma request inteira
	e bem formada, o keep-alive continua valendo e quem decide é a Connection
	pelo header Connection. Antes TODO erro fechava, então um simples 404
	derrubava a conexão, o que o nginx não faz e o pipelining não perdoa. */
static bool	isConnectionFatal(int code) {
	switch (code) {
		case 400: // framing quebrado, não dá pra confiar no resto do buffer
		case 408: // ociosidade estourou, a conexão já era
		case 413: // corpo passou do limite, sobra body não lido no socket
		case 431: // bloco de headers sem fim à vista
		case 500: // estado interno indefinido, mais seguro cortar
		case 501: // método não implementado: parse abortou antes do keep-alive
		case 505: // versão não suportada: idem
			return true;
		default:
			return false;
	}
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
	if (isConnectionFatal(code))
		setCloseAfterSend(true);
}
