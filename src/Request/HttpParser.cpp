/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpParser.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: luide-ca <luide-ca@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/25 16:26:36 by luide-ca          #+#    #+#             */
/*   Updated: 2025/11/25 16:26:36 by luide-ca         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


/*

    TODO: finish recv implementation

*/


#include "HttpParser.hpp"

#include <sstream>     
#include <sys/types.h>
#include <sys/socket.h> 
#include <unistd.h>     
#include <cstring>      
#include <cctype>       
#include <cstdlib>      

// =======================
// Exceptions
// =======================

const char *HttpParser::MethodException::what() const throw()
{
    return "HttpRequest: invalid HTTP method";
}

const char *HttpParser::PathException::what() const throw()
{
    return "HttpRequest: invalid request path";
}

const char *HttpParser::HTTPVersionException::what() const throw()
{
    return "HttpRequest: invalid HTTP version";
}

const char *HttpParser::HeaderException::what() const throw()
{
    return "HttpRequest: invalid header";
}

const char *HttpParser::BodyException::what() const throw()
{
    return "HttpRequest: invalid body length or malformed body";
}

// ParseException with message
HttpParser::ParseException::ParseException(const std::string &msg)
: _msg(msg)
{}

HttpParser::ParseException::ParseException(const ParseException &other)
: std::exception(),
  _msg(other._msg)
{}

HttpParser::ParseException &
HttpParser::ParseException::operator=(const ParseException &other)
{
    if (this != &other)
        _msg = other._msg;
    return *this;
}

HttpParser::ParseException::~ParseException() throw()
{}

const char *HttpParser::ParseException::what() const throw()
{
    return _msg.c_str();
}

// =======================
// Public API
// =======================

void HttpParser::readFromFd(int fd, HttpRequest &req)
{
    req._raw.clear();

    char    buffer[4096];
    ssize_t n;

    while ((n = recv(fd, buffer, sizeof(buffer), 0)) > 0) {
        req._raw.append(buffer, n);
        if (n < (ssize_t)sizeof(buffer))
            break;
    }

    if (req._raw.empty())
        throw ParseException("Empty HTTP request (no data read from socket)");
}

void HttpParser::parse(HttpRequest &req)
{
    std::string::size_type posRequestLineEnd = req._raw.find("\r\n");
    if (posRequestLineEnd == std::string::npos)
        throw ParseException("Malformed HTTP request: missing CRLF after request line");

    std::string::size_type posHeaderEnd = req._raw.find("\r\n\r\n");
    if (posHeaderEnd == std::string::npos)
        throw ParseException("Malformed HTTP request: missing empty line after headers");

    std::string requestLine = req._raw.substr(0, posRequestLineEnd);
    std::string headersBlock = req._raw.substr(
        posRequestLineEnd + 2,
        posHeaderEnd - (posRequestLineEnd + 2)
    );
    std::string body = req._raw.substr(posHeaderEnd + 4);

    parseRequestLine(req, requestLine);
    parseHeadersBlock(req, headersBlock);
    parseBody(req, body);
}

// =======================
// Internal helpers
// =======================

bool HttpParser::isValidPath(const std::string &path)
{
    if (path.empty())
        return (false);
    if (path[0] != '/')
        return (false);
    return (true);
}

bool HttpParser::isValidChunkedBody(const std::string &body)
{
    size_t pos = 0;
    size_t len = body.length();

    while (true)
    {
        size_t lineEnd = body.find("\r\n", pos);
        if (lineEnd == std::string::npos)
            return (false);

        std::string sizeHex = body.substr(pos, lineEnd - pos);

        if (sizeHex.empty())
            return (false);

        char *end;
        long chunkSize = std::strtol(sizeHex.c_str(), &end, 16);

        if (end == sizeHex.c_str() || chunkSize < 0)
            return (false);

        pos = lineEnd + 2;

        if (chunkSize == 0)
        {
            size_t lastCRLF = body.find("\r\n", pos);
            if (lastCRLF == std::string::npos)
                return (false);

            return (true);
        }

        if (pos + chunkSize > len)
            return (false);

        pos += chunkSize;

        if (pos + 2 > len ||
            body[pos] != '\r' ||
            body[pos + 1] != '\n')
            return (false);

        pos += 2;
    }

    return (false);
}

bool HttpParser::isValidBody(const HttpRequest &req, const std::string &body)
{
    std::map<std::string, std::string>::const_iterator itCL =
        req._headers.find("content-length");
    std::map<std::string, std::string>::const_iterator itTE =
        req._headers.find("transfer-encoding");

    if (itCL != req._headers.end() && itTE != req._headers.end())
        return (false);

    if (itCL != req._headers.end())
    {
        char *end;
        long contentLen = std::strtol(itCL->second.c_str(), &end, 10);

        if (end == itCL->second.c_str() || contentLen < 0)
            return (false);

        if (static_cast<size_t>(contentLen) != body.length())
            return (false);

        return (true);
    }

    if (itTE != req._headers.end())
    {
        if (itTE->second != "chunked")
            return (false);

        if (!isValidChunkedBody(body))
            return (false);

        return (true);
    }

    if (req._method == "GET")
        return (true);

    if (!body.empty())
        return (false);
        
    return (true);
}

// =======================
// Internal Parsers
// =======================

void HttpParser::parseRequestLine(HttpRequest &req, const std::string &line)
{
    std::istringstream iss(line);
    std::string method;
    std::string path;
    std::string version;

    if (!(iss >> method >> path >> version))
        throw ParseException("Invalid HTTP request line: \"" + line + "\"");

    setMethod(req, method);
    setPath(req, path);
    setHTTPVersion(req, version);
}

void HttpParser::parseHeadersBlock(HttpRequest &req, const std::string &block)
{
    std::istringstream iss(block);
    std::string line;

    while (std::getline(iss, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.empty())
            continue;

        std::string::size_type pos = line.find(':');
        if (pos == std::string::npos) {
            throw HeaderException();
        }

        std::string key   = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
            value.erase(0, 1);

        req._headers[toLower(key)] = value;
    }
}

void HttpParser::parseBody(HttpRequest &req, const std::string &body)
{
    if (!isValidBody(req, body))
        throw BodyException();
    req._body = body;
}

void HttpParser::setMethod(HttpRequest &req, const std::string &method)
{
    if (method != "GET" && method != "POST" && method != "DELETE")
        throw MethodException();
    req._method = method;
}

void HttpParser::setPath(HttpRequest &req, const std::string &path)
{
    if (!isValidPath(path))
        throw PathException();
    req._path = path;
}

void HttpParser::setHTTPVersion(HttpRequest &req, const std::string &version)
{
    if (version != "HTTP/1.0" && version != "HTTP/1.1")
        throw HTTPVersionException();
    req._httpVersion = version;
}

std::string toLower(const std::string &s)
{
    std::string out = s;
    for (size_t i = 0; i < out.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(out[i]);
        out[i] = static_cast<char>(std::tolower(c));
    }
    return out;
}
