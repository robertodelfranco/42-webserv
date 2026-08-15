#ifndef CGIOUTPUTPARSER_HPP
# define CGIOUTPUTPARSER_HPP

#include <string>

/* O que um script CGI devolve NÃO é HTTP: é um quase-HTTP, com headers
próprios, uma linha em branco e o body. Não existe status line ali - existe
um header opcional "Status:". Traduzir isso numa resposta HTTP de verdade
é trabalho do servidor, e é só isso que esta classe faz.

Sem estado e sem fd nenhum: dá pra testar com strings de mentira, sem forkar
nada. Por isso é static e não uma instância. */
class CgiOutputParser {
	public:
		/* raw = tudo que saiu do stdout do filho. Devolve false quando a saída
		não é um CGI válido (sem bloco de headers), e aí o caller responde 502
		- foi o script que se comportou mal, não o cliente. */
		static bool	toHttpResponse(const std::string& raw, bool keepAlive,
								   std::string& out);

	private:
		CgiOutputParser();
		CgiOutputParser(const CgiOutputParser& other);
		CgiOutputParser& operator=(const CgiOutputParser& other);
		~CgiOutputParser();
};

#endif
