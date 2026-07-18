/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpParser.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: luide-ca <luide-ca@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/25 16:26:00 by luide-ca          #+#    #+#             */
/*   Updated: 2025/11/25 16:26:00 by luide-ca         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_PARSER_HPP
# define HTTP_PARSER_HPP

# include <string>
# include <exception>

# include "HttpRequest.hpp"

class HttpParser
{
    private:
        // Stateless utility: never instantiated.
        HttpParser();
        HttpParser(const HttpParser &other);
        HttpParser &operator=(const HttpParser &other);
        ~HttpParser();

        // ===== Internal helpers =====
        static bool isValidPath(const std::string &path);

        static bool isValidChunkedBody(const std::string &body);
        static bool isValidBody(const HttpRequest &req, const std::string &body);

        // ===== Internal Parsers =====
        static void parseRequestLine(HttpRequest &req, const std::string &line);
        static void parseHeadersBlock(HttpRequest &req, const std::string &block);
        static void parseBody(HttpRequest &req, const std::string &body);

        static void setMethod(HttpRequest &req, const std::string &method);
        static void setPath(HttpRequest &req, const std::string &path);
        static void setHTTPVersion(HttpRequest &req, const std::string &version);

    public:
        // ===== Public API =====
        static void readFromFd(int fd, HttpRequest &req);
        static void parse(HttpRequest &req);

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