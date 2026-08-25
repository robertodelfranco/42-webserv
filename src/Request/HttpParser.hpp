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
	REQ_HEADERS_TOO_LARGE	// bloco de headers sem fim à vista -> 431
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
        bool isValidChunkedBody(const std::string &body);
        bool isValidBody(const HttpRequest &req, const std::string &body);
        
        /*	EDU (AUG22): o findHeader e decodeChunked de Connection. */
        bool findHeader(const std::string& block, const std::string& name,
						std::string& out);
        std::string decodeChunked(const std::string& body);

        // ===== Internal Parsers =====
        void parseRequestLine(HttpRequest &req, const std::string &line);
        void parseHeadersBlock(HttpRequest &req, const std::string &block);
        void parseBody(HttpRequest &req, const std::string &body);

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

        // ===== Exceptions =====
        class MethodException : public std::exception {
        public:
            virtual const char *what() const throw();
        };

        class PathException : public std::exception {
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
