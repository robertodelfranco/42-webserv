#ifndef CGIOUTPUTPARSER_HPP
# define CGIOUTPUTPARSER_HPP

#include <string>

/* O que um script CGI devolve é um quase-HTTP, com headers próprios,
uma linha em branco e o body, então precisamos validar tudo */
class CgiOutputParser {
	public:
		// raw = tudo que saiu do stdout do filho
		static bool	toHttpResponse(const std::string& raw, bool keepAlive, std::string& out);

	private:
		CgiOutputParser();
		CgiOutputParser(const CgiOutputParser& other);
		CgiOutputParser& operator=(const CgiOutputParser& other);
		~CgiOutputParser();
};

#endif
