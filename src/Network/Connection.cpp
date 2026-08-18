#include "Connection.hpp"
#include "Socket.hpp"
#include "Router.hpp"
#include <iostream>
#include <cerrno>
#include <cstdlib>
#include <sstream>
#include <poll.h>
#include <sys/socket.h>
#include "../Response/ErrorResponse.hpp"
#include "../Utils/Color.hpp"
#include "../Utils/Logger.hpp"
#include "../Cgi/CgiProcess.hpp"
#include "../Cgi/CgiOutputParser.hpp"

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif

// Mesmo valor que o nginx usa (large_client_header_buffers).
static const size_t	MAX_HEADER_SIZE = 8192;

// 4096 gerava syscall demais em corpo grande (recv/send de 100MB em CGI).
// Não muda a semântica, só quantos bytes por volta do poll().
static const size_t	IO_BUFFER_SIZE = 65536;

/* ========================= framing provisório =========================
Esses helpers e o checkRequestFraming() SAEM DAQUI quando o HttpParser
tiver um feed(), Connection não deve conhecer de HTTP em nenhum momento (Roberto)

Esse bloco que preenche o _request INTEIRO porque o CGI precisa de tudo isso
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

static bool	parseRequestLineProvisional(const std::string& headers, HttpRequest& req) {
	size_t	firstLineLen = headers.find("\r\n");
	std::string	firstLine = headers.substr(0, firstLineLen);

	size_t	firstSpace = firstLine.find(' ');
	size_t	secondSpace = firstLine.find_last_of(' ');

	if (firstSpace == std::string::npos || secondSpace == firstSpace)
		return false;

	std::string	method = firstLine.substr(0, firstSpace);
	std::string target = firstLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	std::string version = firstLine.substr(secondSpace + 1);

	// corte no '?', é ele que vira o QUERY_STRING do CGI.
	size_t	cutInterr = target.find("?");
	std::string	path = (cutInterr == std::string::npos) ? target : target.substr(0, cutInterr);
	std::string	query = (cutInterr == std::string::npos) ? "" : target.substr(cutInterr + 1);

	req.setRequestLineProvisional(method, path, query, version);
	return true;
}

/* Mesma varredura do findHeader() acima, só que guardando todos os pares em
vez de procurar um. */
static void	parseHeadersProvisional(const std::string& block, HttpRequest& req) {
	std::string::size_type	lineStart = block.find("\r\n");

	if (lineStart == std::string::npos)
		return;
	lineStart += 2;

	while (lineStart < block.size()) {
		std::string::size_type	lineEnd = block.find("\r\n", lineStart);
		if (lineEnd == std::string::npos)
			lineEnd = block.size();

		std::string::size_type	colon = block.find(':', lineStart);
		if (colon != std::string::npos && colon < lineEnd) {
			std::string				name = toLower(block.substr(lineStart, colon - lineStart));
			std::string				value = block.substr(colon + 1, lineEnd - colon - 1);
			std::string::size_type	begin = value.find_first_not_of(" \t");
			std::string::size_type	end = value.find_last_not_of(" \t");

			req.setHeaderProvisional(name, (begin == std::string::npos)
										   ? "" : value.substr(begin, end - begin + 1));
		}
		lineStart = lineEnd + 2;
	}
}

/* Espelho do isValidChunkedBody() do parser */
static std::string	decodeChunkedProvisional(const std::string& body) {
	std::string	out;
	size_t		pos = 0;

	while (true) {
		size_t	lineEnd = body.find("\r\n", pos);
		if (lineEnd == std::string::npos)
			break;

		std::string	sizeHex = body.substr(pos, lineEnd - pos);
		char		*end;
		long		chunkSize = std::strtol(sizeHex.c_str(), &end, 16);

		if (end == sizeHex.c_str() || chunkSize <= 0)
			break; // chunk de tamanho 0 é o fim da lista

		pos = lineEnd + 2;
		if (pos + static_cast<size_t>(chunkSize) > body.size())
			break;

		out.append(body, pos, static_cast<size_t>(chunkSize));
		pos += static_cast<size_t>(chunkSize) + 2; // pula o CRLF do fim do chunk
	}
	return out;
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
  _parser(), _request(), _response(), _state(READING), _matchedLoc(NULL), _cgi(NULL) {
	Socket::setNonBlocking(_fd.get());
}

// delete num CgiProcess mata e colhe o filho (o destrutor dele faz isso),
// então nenhum caminho de morte da conexão deixa processo solto pra trás.
Connection::~Connection() {
	delete _cgi;
}

int	Connection::getFd() const {
	return _fd.get();
}

bool	Connection::hasPendingWrite() const {
	return _state == WRITING && _writeOffset < _writeBuffer.size();
}

bool	Connection::wantsRead() const {
	return _state == READING;
}

bool	Connection::wantsKeepAlive() const {
	return _keepAlive;
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
	/* CGI morre, o cliente recebe um 504 e o filho um SIGKILL. */
	if (_state == CGI_RUNNING) {
		abortCgi(504);
		return;
	}
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
// FUNÇÃO PROVISÓRIA
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

		if (!parseRequestLineProvisional(headers, _request))
			return REQ_BAD;
		parseHeadersProvisional(headers, _request);

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
	_response = Response();
	_matchedLoc = NULL;
	_parser = HttpParser();
	_lastActivity = std::time(NULL); // nova janela de ociosidade
	_state = READING;

	// keep-alive limpo, nenhum resto do CGI anterior atravessa pra próxima
	// request (o finishCgi/abortCgi já deletou, isso é só a garantia).
	if (_cgi) {
		delete _cgi;
		_cgi = NULL;
	}

	Logger::debug() << "fd=" << _fd.get() << " keep-alive: pronto pra proxima ("
		<< _readBuffer.size() << " bytes ja no buffer)";
}

void	Connection::processReadBuffer() {
	// Integração final esperada é _parser.feed(...) no lugar do framing.
	switch (checkRequestFraming()) {
		case REQ_INCOMPLETE:
			break; // volta pro poll() e espera o resto chegar
		case REQ_COMPLETE: {
			/* A request line e os headers já foram preenchidos assim que o
			bloco de headers fechou, o body só dá pra recortar agora, que é
			quando o _requestEnd existe. Chunked entra decodificado. */
			const std::string	body = _readBuffer.substr(_headersEnd, _requestEnd - _headersEnd);

			_request.setBodyProvisional(_chunked ? decodeChunkedProvisional(body) : body);
			decideKeepAlive();
			handleRequest();
			break;
		}
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

	char	buf[IO_BUFFER_SIZE];
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
	// Logger::info() << "fd=" << _fd.get() << " " << _request.getMethod() << " " << _request.getPath();
	_state = PROCESSING;

	_matchedLoc = Router::matchLocation(*_candidate, _request.getPath());

	if (!_matchedLoc) {
		buildErrorResponse(404);
		return ;
	}

	// método barrado pela config é 405, e é decidido antes de escolher handler
	if (!Router::methodAllowed(*_matchedLoc, _request)) {
		Logger::warning() << "fd=" << _fd.get() << " metodo " << _request.getMethod()
			<< " nao permitido em " << _matchedLoc->getPath();
		buildErrorResponse(405);
		return ;
	}

	IRequestHandler* handler = Router::createHandler(*_matchedLoc, _request);
	if (!handler) {
		Logger::error() << "fd=" << _fd.get() << " nenhum handler para "
			<< _request.getPath();
		buildErrorResponse(500);
		return ;
	}

	bool isDone = false;
	try {
		isDone = handler->handle(_request, *_matchedLoc, *this);
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

	// O handler disse "terminei" mas não entregou nada pelo sendResponse ou
	// queueResponse. É bug do handler, não do cliente, então erro interno
	if (_state != WRITING) {
		Logger::error() << "fd=" << _fd.get() << " handler terminou sem responder";
		buildErrorResponse(500);
	}
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

// Única saída de resposta do servidor. A Connection não decide status, 
// ela recebe o Response pronto, resolve o keep-alive e serializa.
void	Connection::sendResponse(const Response& resp) {
	_response = resp;

	// a resposta pode EXIGIR fechar (erro), mas nunca exigir manter aberto
	// isso é decisão da conexão, tomada no decideKeepAlive() a partir da request
	if (_response.getCloseAfterSend())
		_keepAlive = false;
	_response.setCloseAfterSend(!_keepAlive);

	Logger::info() << "fd=" << _fd.get() << " -> " << _response.getStatus()
		<< " " << _response.getStatusMessage() << " ("
		<< _response.getBody().size() << " bytes)";

	queueResponse(_response.toString());
}

/* Só traduz o código na resposta, quem sabe montar página de erro é o ErrorResponse. 
O _matchedLoc pode ser NULL nos erros de framing (parser) antes de casar o location */
void	Connection::buildErrorResponse(int code) {
	sendResponse(ErrorResponse(code, _candidate, _matchedLoc));
}

// ============================== CGI ==============================

void	Connection::adoptCgiProcess(CgiProcess* cgi) {
	delete _cgi; // não deveria haver outro, mas dois filhos numa conexão só seria pior
	_cgi = cgi;
}

bool	Connection::hasCgi() const {
	return _cgi != NULL;
}

CgiProcess*	Connection::getCgi() {
	return _cgi;
}

unsigned short	Connection::getServerPort() const {
	const std::vector<Listen>&	listens = _candidate->getListens();

	return listens.empty() ? 0 : listens[0].port;
}

// função pra decidir qual ponta está o evento (in ou out)
void	Connection::onCgiFdEvent(int fd, short revents) {
	if (!_cgi)
		return;

	if (fd == _cgi->getStdinFd()) {
		// POLLERR/POLLNVAL: a ponta morreu de vez, não adianta tentar escrever.
		// POLLHUP é o filho tendo fechado o stdin dele, write resolve o erro
		// e o onStdinWritable fecha a ponta
		if (revents & (POLLERR | POLLNVAL))
			_cgi->closeStdin();
		else if (revents & (POLLOUT | POLLHUP))
			_cgi->onStdinWritable();
	}
	else if (fd == _cgi->getStdoutFd()) {
		// POLLHUP no stdout é o filho terminando, ainda pode haver bytes no
		// cano, então lemos do mesmo jeito e é o read == 0 que encontra o EOF
		if (revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL))
			_cgi->onStdoutReadable();
	}
	_lastActivity = std::time(NULL);
}

void	Connection::checkCgi(std::time_t now) {
	if (!_cgi)
		return;

	// O prazo é cobrado do fork, não da última atividade porque se o script
	// escreve um byte por segundo pra sempre, ele também precisa morrer.
	if ((_cgi->getPhase() == CgiProcess::RUNNING || _cgi->getPhase() == CgiProcess::REAPING)
		&& now - _cgi->getStartTime() >= CGI_TIMEOUT) {
		Logger::warning() << "fd=" << _fd.get() << " CGI estourou " << CGI_TIMEOUT << "s";
		abortCgi(504);
		return;
	}

	if (_cgi->getPhase() == CgiProcess::REAPING)
		_cgi->checkChild();

	if (_cgi->getPhase() == CgiProcess::FAILED) {
		abortCgi(502);
		return;
	}
	if (_cgi->getPhase() == CgiProcess::COMPLETE)
		finishCgi();
}

void	Connection::finishCgi() {
	std::string	response;

	// Se falhar, as duas são 502 (o erro é do "gateway", não do cliente) ou o
	// script morreu sem dizer nada, ou escreveu algo que não tem bloco de headers.
	if (!CgiOutputParser::toHttpResponse(_cgi->getOutput(), _keepAlive, response)) {
		Logger::warning() << "fd=" << _fd.get() << " CGI devolveu saida invalida (exit="
			<< _cgi->getExitStatus() << ", " << _cgi->getOutput().size() << " bytes)";
		delete _cgi;
		_cgi = NULL;
		buildErrorResponse(502);
		return;
	}

	Logger::info() << "fd=" << _fd.get() << " CGI ok (exit=" << _cgi->getExitStatus()
		<< ", " << response.size() << " bytes)";
	delete _cgi;
	_cgi = NULL;
	queueResponse(response);
	_lastActivity = std::time(NULL);
}

/* O delete mata o filho se ele ainda estiver vivo, ~CgiProcess dá
SIGKILL e depois a conexão volta ao "normal" */
void	Connection::abortCgi(int code) {
	delete _cgi;
	_cgi = NULL;
	buildErrorResponse(code);
	_lastActivity = std::time(NULL);
}

void	Connection::dropCgi() {
	delete _cgi;
	_cgi = NULL;
}

void	Connection::onCgiClientEvent() {
	char	buf[IO_BUFFER_SIZE];
	ssize_t	n = recv(_fd.get(), buf, sizeof(buf), 0);

	if (n > 0) {
		_readBuffer.append(buf, static_cast<size_t>(n));
		_lastActivity = std::time(NULL);
		return;
	}

	/* n == 0 é o cliente que fechou e então não vai ler nenhuma resposta */
	Logger::debug() << "fd=" << _fd.get() << " cliente desistiu durante o CGI";
	dropCgi();
	_state = CLOSED;
}

int	Connection::remainingBudget(std::time_t now) const {
	if (_state == CGI_RUNNING && _cgi) {
		/* Já vi o EOF e só falta o waitpid retornar. Não há mais fd
		pra acordar o poll(), então o timeout tem que ser 0 pra a
		volta seguinte colher o filho */
		if (_cgi->getPhase() != CgiProcess::RUNNING)
			return 0;

		int	remaining = CGI_TIMEOUT - static_cast<int>(now - _cgi->getStartTime());
		return remaining < 0 ? 0 : remaining;
	}

	int	remaining = CONNECTION_TIMEOUT - static_cast<int>(now - _lastActivity);
	return remaining < 0 ? 0 : remaining;
}
