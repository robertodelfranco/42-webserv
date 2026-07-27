#include "Connection.hpp"
#include "Socket.hpp"
#include "../Utils/Color.hpp"
#include "iostream"
#include <sys/socket.h>

// Só guarda o fd (já aceito em outro lugar) e a lista de Server
// candidatos desse endpoint, nenhuma leitura/escrita acontece aqui.
// Fd que chega aqui vem cru do accept, por isso setNonBlocking.
Connection::Connection(int fd, const ServerConfig* candidate)
: _fd(fd), _readBuffer(), _writeBuffer(), _candidate(candidate),
  _lastActivity(std::time(NULL)), _request(), _parser(), _closeRequested(false) {
	Socket::setNonBlocking(_fd.get());
}

Connection::~Connection() {}

int	Connection::getFd() const {
	return _fd.get();
}

bool	Connection::hasPendingWrite() const {
	return !_writeBuffer.empty();
}

bool	Connection::isClosing() const {
	return _closeRequested;
}

std::time_t	Connection::getLastActivity() const {
	return _lastActivity;
}

// O poll() já garantiu que há algo pra ler quando chega aqui.
void	Connection::onReadable() {
	char	buf[4096];
	int		n = recv(_fd.get(), buf, sizeof(buf), 0);

	std::cout << Color::YELLOW << "HI" << Color::RESET << std::endl;
	// n == 0  -> cliente fechou o lado dele (EOF).
	// n <  0  -> erro. (A norma proíbe olhar errno depois de recv, então
	//            qualquer retorno <= 0 vira "fecha a conexão" e ponto.)
	if (n <= 0) {
		_closeRequested = true;
		return;
	}

	_readBuffer.append(buf, n);
	_lastActivity = std::time(NULL);

	// Entrega os bytes acumulados. Não olho o conteúdo aqui: é o
	// handleRequest() que decide o que virou resposta.
	if (!hasPendingWrite())
		handleRequest();
}

// O poll() só devolve POLLOUT quando hasPendingWrite() era true, então
// aqui sempre há algo no _writeBuffer pra mandar.
void	Connection::onWritable() {
	ssize_t	n = send(_fd.get(), _writeBuffer.data(), _writeBuffer.size(), 0);

	std::cout << Color::RED << "BYE" << Color::RESET << std::endl;

	if (n < 0) {
		_closeRequested = true;
		return;
	}

	// send() non-blocking pode aceitar só parte do buffer: desconta
	// exatamente o que saiu e deixa o resto pra próxima volta.
	_writeBuffer.erase(0, n);
	_lastActivity = std::time(NULL);

	// Esvaziou = resposta inteira foi enviada. Sem keep-alive ainda,
	// então encerra. (Aqui é onde, depois, você decide reabrir pra ler
	// a próxima request no mesmo fd em vez de fechar.)
	if (_writeBuffer.empty())
		_closeRequested = true;
}

// PROVISÓRIO: resposta fixa, só pra fechar o fluxo de bytes de ponta a
// ponta (curl -> accept -> recv -> send -> close).
// FINAL: Essa função repassa _readBuffer pro parser e recebe a
// string de resposta pronta, sem que onReadable/onWritable mudem.
void	Connection::handleRequest() {
	try {
		// HttpParser::Status status = _parser.parse(_readBuffer, _request);
		_readBuffer.clear();

		// if (status == HttpParser::INCOMPLETE) // Só uma suposição
			// return; // volta pra onReadable
		
		// COMPLETE: monta _writeBuffer usando só os getters de _request
		// _writeBuffer = buildResponse(_request); 
	} catch (const std::exception& e) {
		// _writeBuffer = buildErrorResponse(400); // ou o código certo pra cada exceção
		_closeRequested = true;
	}
	_writeBuffer =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 13\r\n"
		"Connection: close\r\n"
		"\r\n"
		"Hello, world\n";
	_readBuffer.clear();
}
