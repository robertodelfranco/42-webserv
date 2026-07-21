#include "Connection.hpp"
#include "Socket.hpp"

// Só guarda o fd (já aceito em outro lugar) e a lista de Server
// candidatos desse endpoint, nenhuma leitura/escrita acontece aqui.
// Fd que chega aqui vem cru do accept, por isso setNonBlocking.
Connection::Connection(int fd, const Server* candidate)
: _fd(fd), _readBuffer(), _writeBuffer(), _candidate(candidate) {
	Socket::setNonBlocking(_fd.get());
}

Connection::~Connection() {}

int	Connection::getFd() const {
	return _fd.get();
}

bool	Connection::hasPendingWrite() const {
	return !_writeBuffer.empty();
}
