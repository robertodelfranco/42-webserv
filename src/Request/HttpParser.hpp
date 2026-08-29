/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpParser.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eduribei <eduribei@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/25 16:26:00 by luide-ca          #+#    #+#             */
/*   Updated: 2026/08/22 13:42:18 by eduribei         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPPARSER_HPP
# define HTTPPARSER_HPP

# include <string>
# include <exception>

# include "HttpRequest.hpp"

/*	EDU (AUG22): tirei isso do connection e mudei para um define em vez de uma
	constante, mas agora estou me perguntando qual é a diferença entre usar um
	define e uma contante global no nosso caso... */
// Mesmo valor que o nginx usa (large_client_header_buffers).
# define MAX_HEADER_SIZE 8192

/*	EDU (AUG22): isso também veio do connection */
// "os bytes que tenho no _readBuffer já formam uma request inteira?"
// só o Content-Length diz onde parar de ler.
enum RequestStatus {
	REQ_INCOMPLETE,			// faltam bytes, volta pro poll()
	REQ_COMPLETE,			// mensagem inteira no buffer
	REQ_BAD,				// framing impossível de interpretar -> 400
	REQ_TOO_LARGE,			// body acima do client_max_body_size -> 413
	REQ_HEADERS_TOO_LARGE,	// bloco de headers sem fim à vista -> 431
	REQ_UNSUPPORTED_TRANSFER // Transfer-Encoding que não sabemos desmontar -> 501
};

class HttpParser
{
	private:
		std::string         _raw;
		/*	EDU (AUG22): adicionei aqui as variaveis de estado que estavam no
			Connection (_headersEnd, _bodyExpected, etc), agora o parser controla
			o próprio estado e avisa quando termina de receber o request. */
		size_t              _headersEnd;   
		long long           _bodyExpected; 
		size_t              _requestEnd;   
		bool                _chunked;      
		bool                _headersFilled;

		// ===== Internal helpers =====
		bool isValidPath(const std::string &path);

		/*	Nessa ordem: decodifica %XX e só depois normaliza.
			Invertido, um "%2e%2e" passa batido pela normalização
			e volta a virar ".." na hora de tocar o disco. */
		bool percentDecode(const std::string &in, std::string &out);
		bool normalizePath(const std::string &in, std::string &out);

		/*	Resultado do passeio pelos chunks. Não dá pra usar RequestStatus
			aqui: "faltam bytes" e "framing quebrado" são as únicas respostas
			que o scanner sabe dar, quem traduz pra HTTP é o feed(). */
		enum ChunkScan {
			CHUNK_OK,			// achou o terminador, o fim da request é conhecido
			CHUNK_NEED_MORE,	// válido até aqui, mas ainda incompleto
			CHUNK_BAD			// tamanho ilegível, CRLF faltando, etc.
		};

		/*	Caminha chunk a chunk a partir de 'from'. Substitui o
			find("\r\n0\r\n\r\n") do feed(), que casava com esses bytes quando
			eles apareciam DENTRO do dado de um chunk e cortava a request no
			lugar errado. Caminhando, o tamanho declarado manda, e dado nenhum
			é confundido com terminador. */
		ChunkScan scanChunked(size_t from, size_t &end) const;


		/*	EDU (AUG22): o findHeader e decodeChunked de Connection. */
		bool findHeader(const std::string& block, const std::string& name,
						std::string& out);
		std::string decodeChunked(const std::string& body);

		// ===== Internal Parsers =====
		void parseRequestLine(HttpRequest &req, const std::string &line);
		void parseHeadersBlock(HttpRequest &req, const std::string &block);

		void setMethod(HttpRequest &req, const std::string &method);
		void setPath(HttpRequest &req, const std::string &path);
		void setHTTPVersion(HttpRequest &req, const std::string &version);

	public:
		// ===== Canonical form =====
		HttpParser();
		HttpParser(const HttpParser &other);
		HttpParser &operator=(const HttpParser &other);
		~HttpParser();

		/*	EDU (AUG22): o metodo feed() e uma maquina de estados. ele consome
			os bytes do poll() e retorna o status (terminou, deu erro, etc). */
		RequestStatus 	feed(const char* data, size_t n, long long maxBodySize);
		size_t			requestEnd() const;
		void			parse(HttpRequest &req);

		/*	Bytes que sobraram depois do fim desta request. O cliente pode
		mandar a próxima colada na primeira (pipelining), e esses bytes
		ficam presos no _raw. Sem isso a Connection não tem como resgatar
		eles antes de trocar o parser. Vazio se não houve REQ_COMPLETE. */
		std::string		leftover() const;

		// ===== Exceptions =====
		class MethodException : public std::exception {
		public:
			virtual const char *what() const throw();
		};

		class PathException : public std::exception {
		public:
			virtual const char *what() const throw();
		};

		/*	Separada da PathException porque o contrato de erro é outro:
			path malformado é 400, path que escapa do root é 403. */
		class PathTraversalException : public std::exception {
		public:
			virtual const char *what() const throw();
		};

		class HTTPVersionException : public std::exception {
		public:
			virtual const char *what() const throw();
		};

		class HeaderException : public std::exception {
		public:
			virtual const char *what() const throw();
		};

		class BodyException : public std::exception {
		public:
			virtual const char *what() const throw();
		};

		class ParseException : public std::exception {
		public:
			ParseException(const std::string &msg);
			ParseException(const ParseException &other);
			ParseException &operator=(const ParseException &other);
			virtual ~ParseException() throw();
			virtual const char *what() const throw();
		private:
			std::string _msg;
		};

};

std::string toLower(const std::string &s);

#endif
