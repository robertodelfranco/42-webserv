#include "Connection.hpp"
#include "Socket.hpp"
#include "Router.hpp"
#include "../Utils/Color.hpp"
#include "iostream"
#include <cerrno>
#include <sys/socket.h>

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif

// Só guarda o fd (já aceito em outro lugar) e a lista de Server
// candidatos desse endpoint, nenhuma leitura/escrita acontece aqui.
// Fd que chega aqui vem cru do accept, por isso setNonBlocking.
Connection::Connection(int fd, const ServerConfig* candidate)
: _fd(fd), _readBuffer(), _writeBuffer(), _candidate(candidate),
  _lastActivity(std::time(NULL)), _state(READING), _request(),
   _parser(), _timedOut(false) {
	Socket::setNonBlocking(_fd.get());
}

Connection::~Connection() {}

int	Connection::getFd() const {
	return _fd.get();
}

bool	Connection::hasPendingWrite() const {
	return !_writeBuffer.empty();
}

bool	Connection::wantsRead() const {
	return !_closeRequested && !_readClosed && _writeBuffer.empty();
}

bool	Connection::isClosing() const {
	return _closeRequested;
}

std::time_t	Connection::getLastActivity() const {
	return _lastActivity;
}

void	Connection::requestClose() {
	_closeRequested = true;
}

void	Connection::onTimeout() {
	_timedOut = true;
	_readClosed = true;
	if (_writeStarted || !_writeBuffer.empty()) {
		_closeRequested = true;
		return;
	}
	buildTimeoutResponse();
	_closeAfterWrite = true;
}

// O poll() garantiu que vale tentar ler. Como o fd é non-blocking,
// drenamos tudo que já estiver disponível até EAGAIN/EWOULDBLOCK.
void	Connection::onReadable() {
	if (!wantsRead())
		return;

	char	buf[4096];
	for (;;) {
		ssize_t	n = recv(_fd.get(), buf, sizeof(buf), 0);

		if (n > 0) {
			_lastActivity = std::time(NULL);

			// Integração final esperada:
			// HttpParser::Status status = _parser.feed(std::string(buf, n));
			// INCOMPLETE: continua aguardando bytes.
			// COMPLETE: handleRequest();
			// ERROR: buildBadRequestResponse();
			_readBuffer.append(buf, n);

			// Fallback provisório enquanto a API final do parser não está
			// nesta branch: headers completos já fecham o ciclo read->write.
			if (_readBuffer.find("\r\n\r\n") != std::string::npos) {
				handleRequest();
				_closeAfterWrite = true;
				return;
			}
			continue;
		}
		if (n == 0) {
			_readClosed = true;
			if (_writeBuffer.empty())
				_closeRequested = true;
			return;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		buildBadRequestResponse();
		_closeAfterWrite = true;
		return;
	}
}

// O poll() só devolve POLLOUT quando hasPendingWrite() era true, então
// aqui sempre há algo no _writeBuffer pra mandar.
void	Connection::onWritable() {
	while (!_writeBuffer.empty()) {
		ssize_t	n = send(_fd.get(), _writeBuffer.data(), _writeBuffer.size(), MSG_NOSIGNAL);
    
    if (n < 0) {
        _state = CLOSED;
        return;
    }
    
    if (n == 0) {
    		_closeRequested = true;
        return;
    }

		if (n > 0) {
			_writeStarted = true;
			_writeBuffer.erase(0, n);
			_lastActivity = std::time(NULL);
		}
	}

	// Esvaziou = resposta inteira foi enviada. Sem keep-alive ainda,
	// então encerra. (Aqui é onde, depois, você decide reabrir pra ler
	// a próxima request no mesmo fd em vez de fechar.)
	if (_closeAfterWrite || _writeBuffer.empty())
		_closeRequested = true;
}


void	Connection::handleRequest() {
	const Location* matchedLoc = Router::matchLocation(*_candidate,
													   _request.getPath());
 
	IRequestHandler* handler = Router::createHandler(*matchedLoc, _request);

	bool isDone = handler->handle(_request, *matchedLoc, *this);

    if (isDone) {
        _state = WRITING;
    } else {
        _state = CGI_RUNNING;
    }

    delete handler;
}


// getters e setters do estado de processamento do request pelo hanlder.
State	Connection::getState() const {
	return _state;
}

void	Connection::setState(State newState) {
	_state = newState;

// // PROVISÓRIO: resposta fixa, só pra fechar o fluxo de bytes de ponta a
// // ponta (curl -> accept -> recv -> send -> close).
// // FINAL: Essa função repassa _readBuffer pro parser e recebe a
// // string de resposta pronta, sem que onReadable/onWritable mudem.
// void	Connection::handleRequest() {
// 	try {
// 		// HttpParser::Status status = _parser.parse(_readBuffer, _request);
// 		_readBuffer.clear();

// 		// if (status == HttpParser::INCOMPLETE) // Só uma suposição
// 			// return; // volta pra onReadable
		
// 		// COMPLETE: monta _writeBuffer usando só os getters de _request
// 		// _writeBuffer = buildResponse(_request); 
// 	} catch (const std::exception& e) {
// 		// _writeBuffer = buildErrorResponse(400); // ou o código certo pra cada exceção
// 		_closeRequested = true;
// 	}
// 	_writeBuffer =
// 		"HTTP/1.1 200 OK\r\n"
// 		"Content-Length: 13\r\n"
// 		"Connection: close\r\n"
// 		"\r\n"
// 		"Hello, world\n";
// 	_readBuffer.clear();

}

void	Connection::buildTimeoutResponse() {
	_writeBuffer =
		"HTTP/1.1 408 Request Timeout\r\n"
		"Content-Length: 20\r\n"
		"Connection: close\r\n"
		"\r\n"
		"408 Request Timeout\n";
}

void	Connection::buildBadRequestResponse() {
	_readClosed = true;
	_writeBuffer =
		"HTTP/1.1 400 Bad Request\r\n"
		"Content-Length: 16\r\n"
		"Connection: close\r\n"
		"\r\n"
		"400 Bad Request\n";
}
