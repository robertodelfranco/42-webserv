#include "Connection.hpp"
#include "Socket.hpp"
#include "Router.hpp"
#include "../Config/Color.hpp"
#include "iostream"
#include <cerrno>
#include <sstream>
#include <sys/socket.h>
#include "../Utils/Logger.hpp"

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif

// Mesmo valor que o nginx usa (large_client_header_buffers).
static const size_t	MAX_HEADER_SIZE = 8192;

/* ========================= framing provisório =========================
Esses dois helpers e o checkRequestFraming() SAEM DAQUI quando o HttpParser
tiver um feed(), Connection não deve conhecer de HTTP em nenhum momento (Roberto)
============================================================================ */
static bool	findHeader(const std::string& block, const std::string& name, std::string& out) {
	std::string::size_type	lineStart = block.find("\r\n");

	if (lineStart == std::string::npos)
		return false;
	lineStart += 2;

	while (lineStart < block.size()) {
		std::string::size_type	lineEnd = block.find("\r\n", lineStart);
		if (lineEnd == std::string::npos)
			lineEnd = block.size();

		std::string::size_type	colon = block.find(':', lineStart);
		if (colon != std::string::npos && colon < lineEnd
			&& toLower(block.substr(lineStart, colon - lineStart)) == name) {
			std::string				value = block.substr(colon + 1, lineEnd - colon - 1);
			std::string::size_type	begin = value.find_first_not_of(" \t");
			std::string::size_type	end = value.find_last_not_of(" \t");

			out = (begin == std::string::npos) ? "" : value.substr(begin, end - begin + 1);
			return true;
		}
		lineStart = lineEnd + 2;
	}
	return false;
}

static bool	parseContentLength(const std::string& raw, long long& out) {
	if (raw.empty() || raw.find_first_not_of("0123456789") != std::string::npos)
		return false;

	std::istringstream	iss(raw);
	iss >> out;
	return !iss.fail();
}

/* ======================= fim do framing ========================== */

// Só guarda o fd (já aceito em outro lugar) e a lista de Server
// candidatos desse endpoint, nenhuma leitura/escrita acontece aqui.
Connection::Connection(int fd, const ServerConfig* candidate)
: _fd(fd), _readBuffer(), _headersEnd(std::string::npos), _bodyExpected(-1),
  _requestEnd(0), _chunked(false), _keepAlive(false), _writeBuffer(),
  _writeOffset(0), _candidate(candidate), _lastActivity(std::time(NULL)),
  _parser(), _request(), _response(), _state(READING) {
	Socket::setNonBlocking(_fd.get());
}

Connection::~Connection() {}

int	Connection::getFd() const {
	return _fd.get();
}

bool	Connection::hasPendingWrite() const {
	return _state == WRITING && _writeOffset < _writeBuffer.size();
}

bool	Connection::wantsRead() const {
	return _state == READING;
}

bool	Connection::isClosing() const {
	return _state == CLOSED;
}

std::time_t	Connection::getLastActivity() const {
	return _lastActivity;
}

void	Connection::requestClose() {
	_state = CLOSED;
}

void	Connection::onTimeout() {
	if (_state == CLOSED)
		return;
	if (_state != READING) {
		_state = CLOSED;
		return;
	}
	buildErrorResponse(408);
	_lastActivity = std::time(NULL);
}

// Decide se o _readBuffer já contém uma request inteira. É chamada depois de
// cada recv, depois de achado o fim dos headers, _headersEnd/_bodyExpected 
// ficam guardados e não recalculamos mais nada.
Connection::RequestStatus	Connection::checkRequestFraming() {
	if (_headersEnd == std::string::npos) {
		_headersEnd = _readBuffer.find("\r\n\r\n");
		if (_headersEnd == std::string::npos) {
			if (_readBuffer.size() > MAX_HEADER_SIZE)
				return REQ_HEADERS_TOO_LARGE;
			return REQ_INCOMPLETE;
		}
		_headersEnd += 4; // agora aponta pro primeiro byte do body

		const std::string	headers = _readBuffer.substr(0, _headersEnd);
		std::string			value;

		decideKeepAlive();

		if (findHeader(headers, "transfer-encoding", value) && toLower(value) == "chunked")
			_chunked = true;
		else if (findHeader(headers, "content-length", value)) {
			if (!parseContentLength(value, _bodyExpected))
				return REQ_BAD;
		}
		else
			_bodyExpected = 0; // sem nenhum dos dois headers = sem body (GET, DELETE)
	}

	const long long	limit = _candidate->getBodySize();
	const long long	bodySoFar = static_cast<long long>(_readBuffer.size() - _headersEnd);

	if (_chunked) {
		// Provisóriamente, desmembrar os chunks é trabalho do HttpParser.
		if (bodySoFar > limit)
			return REQ_TOO_LARGE;
		if (_readBuffer.compare(_headersEnd, 5, "0\r\n\r\n") == 0) {
			_requestEnd = _headersEnd + 5;
			return REQ_COMPLETE;
		}
		std::string::size_type	last = _readBuffer.find("\r\n0\r\n\r\n", _headersEnd);
		if (last != std::string::npos) {
			_requestEnd = last + 7;
			return REQ_COMPLETE;
		}
		return REQ_INCOMPLETE;
	}

	if (_bodyExpected > limit)
		return REQ_TOO_LARGE;
	if (bodySoFar >= _bodyExpected) {
		// o resetForNextRequest() vai preservar os bytes recebidos a mais
		_requestEnd = _headersEnd + static_cast<size_t>(_bodyExpected);
		return REQ_COMPLETE;
	}
	return REQ_INCOMPLETE;
}

void	Connection::decideKeepAlive() {
	const std::string	value = toLower(_request.getHeader("connection"));

	_keepAlive = _request.getHTTPVersion() != "HTTP/1.0";
	if (value.find("close") != std::string::npos)
		_keepAlive = false;
	else if (value.find("keep-alive") != std::string::npos)
		_keepAlive = true;
}

void	Connection::resetForNextRequest() {
	_readBuffer.erase(0, _requestEnd);
	_headersEnd = std::string::npos;
	_bodyExpected = -1;
	_requestEnd = 0;
	_chunked = false;
	_writeBuffer.clear();
	_writeOffset = 0;
	_request = HttpRequest();
	_parser = HttpParser();
	_lastActivity = std::time(NULL); // nova janela de ociosidade
	_state = READING;

	Logger::debug() << "fd=" << _fd.get() << " keep-alive: pronto pra proxima ("
		<< _readBuffer.size() << " bytes ja no buffer)";
}

void	Connection::processReadBuffer() {
	// Integração final esperada é _parser.feed(...) no lugar do framing.
	switch (checkRequestFraming()) {
		case REQ_INCOMPLETE:
			break; // volta pro poll() e espera o resto chegar
		case REQ_COMPLETE:
			handleRequest();
			break;
		case REQ_TOO_LARGE:
			Logger::warning() << "fd=" << _fd.get() << " body acima do limite de "
				<< _candidate->getBodySize() << " bytes";
			buildErrorResponse(413);
			break;
		case REQ_HEADERS_TOO_LARGE:
			buildErrorResponse(431);
			break;
		case REQ_BAD:
			buildErrorResponse(400);
			break;
	}
}

void	Connection::onReadable() {
	if (!wantsRead())
		return;

	char	buf[4096];
	ssize_t	n = recv(_fd.get(), buf, sizeof(buf), 0);

	if (n < 0) {
		Logger::debug() << "fd=" << _fd.get() << " recv falhou, encerrando";
		_state = CLOSED;
		return;
	}
	if (n == 0) {
		Logger::debug() << "fd=" << _fd.get() << " EOF do cliente";
		_state = CLOSED;
		return;
	}

	_lastActivity = std::time(NULL);
	_readBuffer.append(buf, n);
	Logger::debug() << "fd=" << _fd.get() << " recv " << n << " bytes (buffer "
		<< _readBuffer.size() << ")";

	processReadBuffer();
}

// Mesma regra do recv que só pode ler/enviar vigiado pelo poll()
void	Connection::onWritable() {
	ssize_t	n = send(_fd.get(), _writeBuffer.data() + _writeOffset,
					 _writeBuffer.size() - _writeOffset, MSG_NOSIGNAL);

	if (n <= 0) {
		Logger::debug() << "fd=" << _fd.get() << " send falhou, encerrando";
		_state = CLOSED;
		return;
	}

	_writeOffset += static_cast<size_t>(n);
	_lastActivity = std::time(NULL);
	Logger::debug() << "fd=" << _fd.get() << " send " << n << " bytes, faltam "
		<< (_writeBuffer.size() - _writeOffset);

	if (_writeOffset != _writeBuffer.size())
		return; // ainda falta resposta pra mandar, o poll() chama de novo

	if (!_keepAlive) {
		_state = CLOSED;
		return;
	}

	resetForNextRequest();
	if (!_readBuffer.empty())
		processReadBuffer();
}


void	Connection::handleRequest() {
	Logger::info() << "fd=" << _fd.get() << " " << _request.getMethod() << " " << _request.getPath();
	_state = PROCESSING;
	const Location* matchedLoc = Router::matchLocation(*_candidate, _request.getPath());

	if (!matchedLoc) {
		buildErrorResponse(404);
		return ;
	}

	IRequestHandler* handler = Router::createHandler(*matchedLoc, _request);
	if (!handler) {
		buildErrorResponse(500); // nenhum handler soube tratar
		return ;
	}

	bool isDone = false;
	try {
		isDone = handler->handle(_request, *matchedLoc, *this);
	} catch (const std::exception& e) {
		Logger::error() << "fd=" << _fd.get() << " handler lançou: " << e.what();
		delete handler;
		buildErrorResponse(500);
		return ;
	}
	delete handler;

	if (!isDone) {
		_state = CGI_RUNNING;
		return ;
	}

	// quando a classe de Response existir, aqui vira queueResponse(_response.toString()).
	if (_writeOffset == _writeBuffer.size())
		buildErrorResponse(500);
	else
		_state = WRITING;
}

State	Connection::getState() const {
	return _state;
}

void	Connection::setState(State newState) {
	_state = newState;
}

void	Connection::queueResponse(const std::string& raw) {
	_writeBuffer = raw;
	_writeOffset = 0;
	_state = WRITING;
}

static const char*	errorPhrase(int code) {
	switch (code) {
		case 400:
			return "400 - Bad Request";
		case 404:
			return "404 - Not Found";
		case 408:
			return "408 - Request Timeout";
		case 413:
			return "413 - Content Too Large";
		case 431:
			return "431 - Request Header Fields Too Large";
		default:
			return "500 - Internal Server Error";
	}
}

// resposta chumbada por enquanto para funcionar os testers e o servidor
void	Connection::buildErrorResponse(int code) {
	const std::string	phrase = errorPhrase(code);

	// Erro sempre encerra: sem Content-Length o cliente só sabe que o body
	// acabou quando a conexão fecha. Quando o ErrorResponse assumir e mandar
	// o Content-Length, 404 e 500 podem voltar a sobreviver ao keep-alive.
	_keepAlive = false;

	Logger::info() << "fd=" << _fd.get() << " -> " << phrase;
	queueResponse("HTTP/1.1 " + phrase + "\r\nConnection: close\r\n\r\n" + phrase + "\n");
}
