/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpParser.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eduribei <eduribei@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/25 16:26:36 by luide-ca          #+#    #+#             */
/*   Updated: 2026/08/22 14:45:28 by eduribei         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#include "HttpParser.hpp"
#include <sstream>     
#include <cstdlib>      


// =======================
// OCF
// =======================

/* EDU (Aug22): inicializando as novas variaveis de estado no construtor. */
HttpParser::HttpParser() 
: _raw(), _headersEnd(std::string::npos), _bodyExpected(-1), 
  _requestEnd(0), _chunked(false), _headersFilled(false) {}

HttpParser::HttpParser(const HttpParser &other) { *this = other; }

HttpParser &HttpParser::operator=(const HttpParser &other) { 
    if (this != &other) {
        _raw = other._raw;
        _headersEnd = other._headersEnd;
        _bodyExpected = other._bodyExpected;
        _requestEnd = other._requestEnd;
        _chunked = other._chunked;
        _headersFilled = other._headersFilled;
    }
    return *this; 
}

HttpParser::~HttpParser() {}

// =======================
// Public API
// =======================

/* EDU (AUG22): aqui esta a maquina de estados, o feed. ela vai lendo os bytes crus,
acha o fim dos headers e verifica o tamanho do body ou chunks. */
RequestStatus HttpParser::feed(const char* data, size_t n, long long maxBodySize)
{
    _raw.append(data, n);

    if (!_headersFilled) {
        _headersEnd = _raw.find("\r\n\r\n");
        if (_headersEnd == std::string::npos) {
            if (_raw.size() > MAX_HEADER_SIZE)
                return REQ_HEADERS_TOO_LARGE;
            return REQ_INCOMPLETE;
        }
        _headersEnd += 4; 
        _headersFilled = true;

        std::string headers = _raw.substr(0, _headersEnd);
        std::string value;

        if (findHeader(headers, "transfer-encoding", value) && toLower(value) == "chunked") {
            _chunked = true;
        } else if (findHeader(headers, "content-length", value)) {
            char *end;
            _bodyExpected = std::strtoll(value.c_str(), &end, 10);
            if (end == value.c_str() || _bodyExpected < 0) return REQ_BAD;
        } else {
            _bodyExpected = 0; 
        }
    }

    long long bodySoFar = static_cast<long long>(_raw.size() - _headersEnd);

    if (_chunked) {
        if (bodySoFar > maxBodySize) return REQ_TOO_LARGE;
        
        if (_raw.compare(_headersEnd, 5, "0\r\n\r\n") == 0) {
            _requestEnd = _headersEnd + 5;
            return REQ_COMPLETE;
        }
        std::string::size_type last = _raw.find("\r\n0\r\n\r\n", _headersEnd);
        if (last != std::string::npos) {
            _requestEnd = last + 7;
            return REQ_COMPLETE;
        }
        return REQ_INCOMPLETE;
    }

    if (_bodyExpected > maxBodySize) return REQ_TOO_LARGE;
    if (bodySoFar >= _bodyExpected) {
        _requestEnd = _headersEnd + static_cast<size_t>(_bodyExpected);
        return REQ_COMPLETE;
    }

    return REQ_INCOMPLETE;
}

size_t HttpParser::requestEnd() const {
    return _requestEnd;
}

/* EDU (AUG22): O parse() agora só roda de verdade quando o feed() avisa que a request ta completa (_headersFilled e _requestEnd > 0). Nada de parsear pela metade! */
void HttpParser::parse(HttpRequest &req)
{
    if (!_headersFilled || _requestEnd == 0)
        return;

    std::string::size_type posRequestLineEnd = _raw.find("\r\n");
    std::string requestLine = _raw.substr(0, posRequestLineEnd);
    std::string headersBlock = _raw.substr(
        posRequestLineEnd + 2, 
        _headersEnd - (posRequestLineEnd + 2) - 4
    );
    
    std::string body = _raw.substr(_headersEnd, _requestEnd - _headersEnd);

    parseRequestLine(req, requestLine);
    parseHeadersBlock(req, headersBlock);

    if (_chunked) {
        req.body_ = decodeChunked(body);
    } else {
        req.body_ = body;
    }
}

// =======================
// Internal helpers
// =======================

/* EDU (AUG22): logica do findHeader e decodeChunked trazida da Connection
para o parser mastigar os chunks sozinho. */
bool HttpParser::findHeader(const std::string& block, const std::string& name,
							std::string& out) {
    std::string::size_type lineStart = block.find("\r\n");
    if (lineStart == std::string::npos) return false;
    lineStart += 2;

    while (lineStart < block.size()) {
        std::string::size_type lineEnd = block.find("\r\n", lineStart);
        if (lineEnd == std::string::npos) lineEnd = block.size();

        std::string::size_type colon = block.find(':', lineStart);
        if (colon != std::string::npos && colon < lineEnd && toLower(block.substr(lineStart, colon - lineStart)) == name) {
            std::string value = block.substr(colon + 1, lineEnd - colon - 1);
            std::string::size_type begin = value.find_first_not_of(" \t");
            std::string::size_type end = value.find_last_not_of(" \t");
            out = (begin == std::string::npos) ? "" : value.substr(begin, end - begin + 1);
            return true;
        }
        lineStart = lineEnd + 2;
    }
    return false;
}

std::string HttpParser::decodeChunked(const std::string& body) {
    std::string out;
    size_t pos = 0;

    while (true)
	{
        size_t lineEnd = body.find("\r\n", pos);
        if (lineEnd == std::string::npos)
			break;

        std::string sizeHex = body.substr(pos, lineEnd - pos);
        char *end;
        long chunkSize = std::strtol(sizeHex.c_str(), &end, 16);

        if (end == sizeHex.c_str() || chunkSize <= 0)
			break; 

        pos = lineEnd + 2;

        if (pos + static_cast<size_t>(chunkSize) > body.size())
			break;

        out.append(body, pos, static_cast<size_t>(chunkSize));
        pos += static_cast<size_t>(chunkSize) + 2; 
    }
    return out;
}

bool HttpParser::isValidPath(const std::string &path)
{
    if (path.empty() || path[0] != '/') return false;
    return true;
}

// =======================
// Internal Parsers
// =======================

void HttpParser::parseRequestLine(HttpRequest &req, const std::string &line)
{
    std::istringstream iss(line);
    std::string method;
    std::string target;
    std::string version;

    if (!(iss >> method >> target >> version))
        throw ParseException("Invalid HTTP request line: \"" + line + "\"");

    setMethod(req, method);
    
    size_t cutInterr = target.find("?");
    std::string path = (cutInterr == std::string::npos) ? target : target.substr(0, cutInterr);
    std::string query = (cutInterr == std::string::npos) ? "" : target.substr(cutInterr + 1);

    setPath(req, path);
    req.query_ = query;
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

        req.headers_[toLower(key)] = value;
    }
}

void HttpParser::setMethod(HttpRequest &req, const std::string &method)
{
    if (method != "GET" && method != "POST" && method != "DELETE")
        throw MethodException();
    req.method_ = method;
}

void HttpParser::setPath(HttpRequest &req, const std::string &path)
{
    if (!isValidPath(path)) 
		throw PathException();
    req.path_ = path;
}

void HttpParser::setHTTPVersion(HttpRequest &req, const std::string &version)
{
    if (version != "HTTP/1.0" && version != "HTTP/1.1")
        throw HTTPVersionException();
    req.httpVersion_ = version;
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

/* coloquei as exception no fim (edu) */

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



/////// fim ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/*------------------------------------------------------------------------------
▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀
                                 ▄▄                                                 
                     ▄▄          ██                                                 
███▄███▄ ▄███▄ ▄████ ██ ▄█▀   ▄████ ▄█▀█▄   ████▄  ▀▀█▄ ██   ██                     
██ ██ ██ ██ ██ ██    ████     ██ ██ ██▄█▀   ██ ▀▀ ▄█▀██ ██ █ ██                     
██ ██ ██ ▀███▀ ▀████ ██ ▀█▄   ▀████ ▀█▄▄▄   ██    ▀█▄██  ██▀██                      
                                                              ▄▄▄▄▄▄▄▄              
                                                                                    

▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ */

std::string raw_mock(int type) {

	std::string raw1 =
		"GET /index.html HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n";

	std::string raw2 =
		"GET /uploads/ HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n";

	std::string raw3 =
		"GET /uploads HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n";

	std::string raw4 =
		"POST /uploads/novo.txt HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 27\r\n"
		"\r\n"
		"conteudo do arquivo enviado";

	std::string raw5 =
		"DELETE /uploads/novo.txt HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n";

	switch (type) {
		case 1: return raw1;
		case 2: return raw2;
		case 3: return raw3;
		case 4: return raw4;
		case 5: return raw5;
		default: return "";
	}
}
