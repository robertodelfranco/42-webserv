/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpParser.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eduribei <eduribei@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/25 16:26:00 by luide-ca          #+#    #+#             */
/*   Updated: 2026/07/18 20:54:05 by eduribei         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPPARSER_HPP
# define HTTPPARSER_HPP

# include <string>
# include <exception>

# include "HttpRequest.hpp"

class HttpParser
{
    private:
        // ===== Raw data ===== 
        /* esse raw_ é o request bruto lido diretamente do fd do socket. o
		método readFromFd() fica responsável por ir preenchendo. somente depois
		que a leitura termina, o método parse() entra em ação para preencher
		os campos do HttpRequest. só funciona porque o HttpParser recebe um
		HttpRequest como ref& e pode editar os privados porque é "friend". */
		std::string raw_;

        // ===== Internal helpers =====
        bool isValidPath(const std::string &path);
        bool isValidChunkedBody(const std::string &body);
        bool isValidBody(const HttpRequest &req, const std::string &body);

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

		// ===== Public API =====
		void readFromFd(int fd);
		void parse(HttpRequest &req);

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
