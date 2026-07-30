/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTPRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rheringe <rheringe@student.42sp.org.br>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/24 17:26:00 by luide-ca          #+#    #+#             */
/*   Updated: 2025/12/15 17:34:01 by rheringe         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_REQUEST_HPP
# define HTTP_REQUEST_HPP

# include <string>
# include <map>
# include <exception>

class HTTPRequest
{
    private:
        // ===== Raw data =====
        std::string _raw;

        // ===== Request line =====
        std::string _method;
        std::string _path;
        std::string _httpVersion;

        // ===== Headers + body_ =====
        std::map<std::string, std::string> _headers;
        std::string                        _body;

        // ===== Internal helpers =====
        bool isValidPath(const std::string &path) const;
        
        bool isValidChunkedbody_(const std::string &body_) const;
        bool isValidbody_(const std::string &body_) const;

        // ===== Internal Parsers =====
        void parseRequestLine(const std::string &line);
        void parseHeadersBlock(const std::string &block);
        void parsebody_(const std::string &body_);

        void setMethod(const std::string &method);
        void setPath(const std::string &path);
        void setHTTPVersion(const std::string &version);

    public:
        // ===== Canonical form =====
        HTTPRequest();
        explicit HTTPRequest(int fd);
        HTTPRequest(const HTTPRequest &other);
        HTTPRequest &operator=(const HTTPRequest &other);
        ~HTTPRequest();

        // ===== Public API =====
        void        readFromFd(int fd);   
        void        parse();                      

        const std::string &getRaw() const;
        const std::string &getMethod() const;
        const std::string &getPath() const;
        const std::string &getHTTPVersion() const;
        const std::map<std::string, std::string> &getHeaders() const;
        std::string        getHeader(const std::string &key) const;
        const std::string &getBody() const;

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

        class body_Exception : public std::exception {
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
