/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpParser.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eduribei <eduribei@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/25 16:26:36 by luide-ca          #+#    #+#             */
/*   Updated: 2026/08/22 19:25:30 by eduribei         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#include "HttpParser.hpp"
#include <sstream>
#include <cstdlib>
#include <vector>   // pilha de segmentos do normalizePath


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

		/* Achou o fim, mas o bloco inteiro pode ter vindo num recv só e
		estourado o limite: aí o teste de cima nunca rodou. Por isso ele
		precisa ser refeito aqui, agora medindo o bloco de headers de
		verdade (_headersEnd) e não o _raw todo, que já inclui body. */
		if (_headersEnd > MAX_HEADER_SIZE)
			return REQ_HEADERS_TOO_LARGE;

		const std::string	headers = _raw.substr(0, _headersEnd);
		std::string			teValue;
		std::string			clValue;

		const bool	hasTE = findHeader(headers, "transfer-encoding", teValue);
		const bool	hasCL = findHeader(headers, "content-length", clValue);

		if (hasTE && hasCL)
			return REQ_BAD;

		if (hasTE) {
			// "gzip", "deflate", "chunked, gzip"... não sabemos desmontar
			if (toLower(teValue) != "chunked")
				return REQ_UNSUPPORTED_TRANSFER;
			_chunked = true;
		} else if (hasCL) {
			char	*end;
			_bodyExpected = std::strtoll(clValue.c_str(), &end, 10);
			/*	*end != '\0' recusa "5abc": o strtoll para no 'a' e devolve 5
				sem reclamar, e aí a gente framaria a request pelo lixo. */
			if (end == clValue.c_str() || *end != '\0' || _bodyExpected < 0)
				return REQ_BAD;
		} else {
			_bodyExpected = 0;
		}

		_headersFilled = true;
	}

	long long bodySoFar = static_cast<long long>(_raw.size() - _headersEnd);

	if (_chunked) {
		if (bodySoFar > maxBodySize) return REQ_TOO_LARGE;

		size_t			end = 0;
		const ChunkScan	scan = scanChunked(_headersEnd, end);

		if (scan == CHUNK_BAD)
			return REQ_BAD; // antes isso pendurava a conexão até o timeout
		if (scan == CHUNK_NEED_MORE)
			return REQ_INCOMPLETE;

		_requestEnd = end;
		return REQ_COMPLETE;
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

std::string HttpParser::leftover() const {
	if (!_headersFilled || _requestEnd == 0 || _requestEnd >= _raw.size())
		return std::string();
	return _raw.substr(_requestEnd);
}

/*	EDU (AUG22): O parse() agora só roda de verdade quando o feed() avisa
	que a request ta completa (_headersFilled e _requestEnd > 0) */
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

// valor de um dígito hexadecimal, -1 se não for um
static int	hexValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/*	Decodifica os %XX do path. false em escape quebrado ("%zz", ou "%4" no
	fim da string) e em %00. O %00 é recusado porque byte nulo é injeção
	de fim de string: um "/foto%00.php" passa por qualquer checagem de 
	extensão como se fosse .php e depois o open() enxerga só "/foto" */
bool HttpParser::percentDecode(const std::string &in, std::string &out) {
	out.clear();
	out.reserve(in.size());

	for (std::string::size_type i = 0; i < in.size(); ++i) {
		if (in[i] != '%') {
			out += in[i];
			continue;
		}

		if (i + 2 >= in.size())
			return false; // "%" ou "%4" sem os dois dígitos

		const int	hi = hexValue(in[i + 1]);
		const int	lo = hexValue(in[i + 2]);
		if (hi < 0 || lo < 0)
			return false;

		const char	decoded = static_cast<char>(hi * 16 + lo);
		if (decoded == '\0')
			return false;

		out += decoded;
		i += 2;
	}
	return true;
}

/*	Colapsa "." , ".." e barras repetidas ("/a//b" = "/a/b"), usando uma
	pilha de segmentos. False quando um ".." tenta desempilhar de uma pilha
	vazia: é o caminho subindo acima da raiz da URI, ou seja, o traversal.
	
	Com o path normalizado aqui, "root + URI" não tem mais como sair do
	root, e nenhum handler precisa repetir essa conta.

	A barra final é preservada de propósito: o autoindex e o redirect 301
	de diretório dependem dela pra saber que a URI aponta pra uma pasta. */
bool HttpParser::normalizePath(const std::string &in, std::string &out)
{
	std::vector<std::string>	stack;
	std::string					segment;
	const bool					trailingSlash = !in.empty() && in[in.size() - 1] == '/';

	// o <= fecha o último segmento, que não termina em '/'
	for (std::string::size_type i = 0; i <= in.size(); ++i) {
		if (i < in.size() && in[i] != '/') {
			segment += in[i];
			continue;
		}

		if (segment == "..") {
			if (stack.empty())
				return false; // subiu acima da raiz
			stack.erase(stack.end() - 1);
		}
		else if (!segment.empty() && segment != ".") {
			stack.push_back(segment); // vazio vem de "//", "." não move nada
		}
		segment.clear();
	}

	out = "/";
	for (size_t i = 0; i < stack.size(); ++i) {
		out += stack[i];
		if (i + 1 < stack.size())
			out += '/';
	}
	if (trailingSlash && !stack.empty())
		out += '/';

	return true;
}

/*	Isso substitui o find("\r\n0\r\n\r\n") de antes, que tinha dois furos:
	casava com esses bytes dentro do dado de um chunk (cortando a request no
	meio) e nunca detectava framing quebrado, deixando a conexão pendurada
	até o timeout de 90s. Antes era feito por parseBody e IsValidChunkedBody
	mas foram retiradas do projeto. */
HttpParser::ChunkScan	HttpParser::scanChunked(size_t from, size_t &end) const
{
	size_t	pos = from;

	while (true) {
		const size_t	lineEnd = _raw.find("\r\n", pos);
		if (lineEnd == std::string::npos)
			return CHUNK_NEED_MORE; // a linha de tamanho ainda está chegando

		std::string	sizeLine = _raw.substr(pos, lineEnd - pos);

		// extensão de chunk ("1a;nome=valor") é legal, o tamanho é o que vem antes do ';'
		const std::string::size_type	semi = sizeLine.find(';');
		if (semi != std::string::npos)
			sizeLine = sizeLine.substr(0, semi);

		if (sizeLine.empty())
			return CHUNK_BAD;

		/*	strtol sozinho aceitaria " +5" e pararia no primeiro caractere
			ruim sem reclamar, então cada dígito é conferido à mão antes. */
		for (size_t i = 0; i < sizeLine.size(); ++i)
			if (hexValue(sizeLine[i]) < 0)
				return CHUNK_BAD;

		char		*endp;
		const long	chunkSize = std::strtol(sizeLine.c_str(), &endp, 16);
		if (chunkSize < 0)
			return CHUNK_BAD;

		pos = lineEnd + 2;

		if (chunkSize == 0) {
			/*	Último chunk. Depois dele podem vir trailers, terminados por
				uma linha vazia, que é o caso comum ("0\r\n\r\n"). */
			while (true) {
				const size_t	trailerEnd = _raw.find("\r\n", pos);
				if (trailerEnd == std::string::npos)
					return CHUNK_NEED_MORE;
				if (trailerEnd == pos) { // linha vazia: acabou
					end = pos + 2;
					return CHUNK_OK;
				}
				pos = trailerEnd + 2; // era um trailer, pula
			}
		}

		// o dado precisa caber inteiro, mais o CRLF que o fecha
		if (pos + static_cast<size_t>(chunkSize) + 2 > _raw.size())
			return CHUNK_NEED_MORE;
		if (_raw[pos + chunkSize] != '\r' || _raw[pos + chunkSize + 1] != '\n')
			return CHUNK_BAD;

		pos += static_cast<size_t>(chunkSize) + 2;
	}
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
	//HEAD entra aqui porque o servidor SABE fazer HEAD (StaticHandler tem
	// handleHEAD). Quem decide se ele é permitido neste location é o Router
	if (method != "GET" && method != "POST"
		&& method != "DELETE" && method != "HEAD")
		throw MethodException();
	req.method_ = method;
}

void HttpParser::setPath(HttpRequest &req, const std::string &path)
{
	/*	No path CRU, antes do decode: assim um "%2Fetc/passwd" (barra inicial
		codificada) morre aqui em vez de virar "/etc/passwd" depois. */
	if (!isValidPath(path))
		throw PathException();

	std::string	decoded;
	if (!percentDecode(path, decoded))
		throw PathException(); // escape quebrado ou %00 -> 400

	std::string	normalized;
	if (!normalizePath(decoded, normalized))
		throw PathTraversalException(); // escapou do root -> 403

	req.path_ = normalized;
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

const char *HttpParser::PathTraversalException::what() const throw()
{
	return "HttpRequest: path escapes the document root";
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
