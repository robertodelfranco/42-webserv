/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTPRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rheringe <rheringe@student.42sp.org.br>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/24 17:26:36 by luide-ca          #+#    #+#             */
/*   Updated: 2025/12/15 17:34:34 by rheringe         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


/*

    TODO: finish recv implementation

*/


#include "HTTPRequest.hpp"

#include <sstream>     
#include <sys/types.h>
#include <sys/socket.h> 
#include <unistd.h>     
#include <cstring>      
#include <cctype>       
#include <cstdlib>      

// =======================
// Canonical form
// =======================

HTTPRequest::HTTPRequest()
: _raw(),
  _method(),
  _path(),
  _httpVersion(),
  _headers(),
  _body()
{}

HTTPRequest::HTTPRequest(int fd)
: _raw(),
  _method(),
  _path(),
  _httpVersion(),
  _headers(),
  _body()
{
    readFromFd(fd);
    parse();
}

HTTPRequest::HTTPRequest(const HTTPRequest &other)
: _raw(other._raw),
  _method(other._method),
  _path(other._path),
  _httpVersion(other._httpVersion),
  _headers(other._headers),
  _body(other._body)
{}

HTTPRequest &HTTPRequest::operator=(const HTTPRequest &other)
{
    if (this != &other) {
        _raw         = other._raw;
        _method      = other._method;
        _path        = other._path;
        _httpVersion = other._httpVersion;
        _headers     = other._headers;
        _body        = other._body;
    }
    return *this;
}

HTTPRequest::~HTTPRequest()
{}

// =======================
// Exceptions
// =======================

const char *HTTPRequest::MethodException::what() const throw()
{
    return "HTTPRequest: invalid HTTP method";
}

const char *HTTPRequest::PathException::what() const throw()
{
    return "HTTPRequest: invalid request path";
}

const char *HTTPRequest::HTTPVersionException::what() const throw()
{
    return "HTTPRequest: invalid HTTP version";
}

const char *HTTPRequest::HeaderException::what() const throw()
{
    return "HTTPRequest: invalid header";
}

const char *HTTPRequest::body_Exception::what() const throw()
{
    return "HTTPRequest: invalid body_ length or malformed body_";
}

// ParseException with message
HTTPRequest::ParseException::ParseException(const std::string &msg)
: _msg(msg)
{}

HTTPRequest::ParseException::ParseException(const ParseException &other)
: std::exception(),
  _msg(other._msg)
{}

HTTPRequest::ParseException &
HTTPRequest::ParseException::operator=(const ParseException &other)
{
    if (this != &other)
        _msg = other._msg;
    return *this;
}

HTTPRequest::ParseException::~ParseException() throw()
{}

const char *HTTPRequest::ParseException::what() const throw()
{
    return _msg.c_str();
}

// =======================
// Public API
// =======================

void HTTPRequest::readFromFd(int fd)
{
    _raw.clear();

    char    buffer[4096];
    ssize_t n;

    while ((n = recv(fd, buffer, sizeof(buffer), 0)) > 0) {
        _raw.append(buffer, n);
        if (n < (ssize_t)sizeof(buffer))
            break;
    }

    if (_raw.empty())
        throw ParseException("Empty HTTP request (no data read from socket)");
}

void HTTPRequest::parse()
{
    std::string::size_type posRequestLineEnd = _raw.find("\r\n");
    if (posRequestLineEnd == std::string::npos)
        throw ParseException("Malformed HTTP request: missing CRLF after request line");

    std::string::size_type posHeaderEnd = _raw.find("\r\n\r\n");
    if (posHeaderEnd == std::string::npos)
        throw ParseException("Malformed HTTP request: missing empty line after headers");

    std::string requestLine = _raw.substr(0, posRequestLineEnd);
    std::string headersBlock = _raw.substr(
        posRequestLineEnd + 2,
        posHeaderEnd - (posRequestLineEnd + 2)
    );
    std::string body_ = _raw.substr(posHeaderEnd + 4);

    parseRequestLine(requestLine);
    parseHeadersBlock(headersBlock);
    parsebody_(body_);
}

const std::string &HTTPRequest::getRaw() const
{
    return _raw;
}

const std::string &HTTPRequest::getMethod() const
{
    return _method;
}

const std::string &HTTPRequest::getPath() const
{
    return _path;
}

const std::string &HTTPRequest::getHTTPVersion() const
{
    return _httpVersion;
}

const std::map<std::string, std::string> &HTTPRequest::getHeaders() const
{
    return _headers;
}

std::string HTTPRequest::getHeader(const std::string &key) const
{
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it == _headers.end())
        return std::string();
    return it->second;
}

const std::string &HTTPRequest::getBody() const
{
    return _body;
}

// =======================
// Internal helpers
// =======================

bool HTTPRequest::isValidPath(const std::string &path) const
{
    if (path.empty())
        return (false);
    if (path[0] != '/')
        return (false);
    return (true);
}

bool HTTPRequest::isValidChunkedbody_(const std::string &body_) const
{
    size_t pos = 0;
    size_t len = body_.length();

    while (true)
    {
        size_t lineEnd = body_.find("\r\n", pos);
        if (lineEnd == std::string::npos)
            return (false);

        std::string sizeHex = body_.substr(pos, lineEnd - pos);

        if (sizeHex.empty())
            return (false);

        char *end;
        long chunkSize = std::strtol(sizeHex.c_str(), &end, 16);

        if (end == sizeHex.c_str() || chunkSize < 0)
            return (false);

        pos = lineEnd + 2;

        if (chunkSize == 0)
        {
            size_t lastCRLF = body_.find("\r\n", pos);
            if (lastCRLF == std::string::npos)
                return (false);

            return (true);
        }

        if (pos + chunkSize > len)
            return (false);

        pos += chunkSize;

        if (pos + 2 > len ||
            body_[pos] != '\r' ||
            body_[pos + 1] != '\n')
            return (false);

        pos += 2;
    }

    return (false);
}

bool HTTPRequest::isValidbody_(const std::string &body_) const
{
    std::map<std::string, std::string>::const_iterator itCL =
        _headers.find("content-length");
    std::map<std::string, std::string>::const_iterator itTE =
        _headers.find("transfer-encoding");

    if (itCL != _headers.end() && itTE != _headers.end())
        return (false);

    if (itCL != _headers.end())
    {
        char *end;
        long contentLen = std::strtol(itCL->second.c_str(), &end, 10);

        if (end == itCL->second.c_str() || contentLen < 0)
            return (false);

        if (static_cast<size_t>(contentLen) != body_.length())
            return (false);

        return (true);
    }

    if (itTE != _headers.end())
    {
        if (itTE->second != "chunked")
            return (false);

        if (!isValidChunkedbody_(body_))
            return (false);

        return (true);
    }

    if (_method == "GET")
        return (true);

    if (!body_.empty())
        return (false);
        
    return (true);
}

// =======================
// Internal Parsers
// =======================

void HTTPRequest::parseRequestLine(const std::string &line)
{
    std::istringstream iss(line);
    std::string method;
    std::string path;
    std::string version;

    if (!(iss >> method >> path >> version))
        throw ParseException("Invalid HTTP request line: \"" + line + "\"");

    setMethod(method);
    setPath(path);
    setHTTPVersion(version);
}

void HTTPRequest::parseHeadersBlock(const std::string &block)
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

        _headers[toLower(key)] = value;
    }
}

void HTTPRequest::parsebody_(const std::string &body_)
{
    if (!isValidbody_(body_))
        throw body_Exception();
    _body = body_;
}

void HTTPRequest::setMethod(const std::string &method)
{
    if (method != "GET" && method != "POST" && method != "DELETE")
        throw MethodException();
    _method = method;
}

void HTTPRequest::setPath(const std::string &path)
{
    if (!isValidPath(path))
        throw PathException();
    _path = path;
}

void HTTPRequest::setHTTPVersion(const std::string &version)
{
    if (version != "HTTP/1.0" && version != "HTTP/1.1")
        throw HTTPVersionException();
    _httpVersion = version;
}

static std::string toLower(const std::string &s)
{
    std::string out = s;
    for (size_t i = 0; i < out.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(out[i]);
        out[i] = static_cast<char>(std::tolower(c));
    }
    return out;
}
