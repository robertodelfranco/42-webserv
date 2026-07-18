#include "Connection.hpp"

// Só guarda o fd (já aceito em outro lugar) e a lista de Server
// candidatos desse endpoint, nenhuma leitura/escrita acontece aqui.
Connection::Connection(int fd, const std::vector<const Server*>& candidates)
: _fd(fd), _readBuffer(), _writeBuffer(), _candidates(candidates) {}

// Corpo vazio de propósito: o destrutor do membro _fd (FileDescriptor)
// já fecha o fd sozinho.
Connection::~Connection() {}

// Repassa o fd pra quem monta o array de pollfd do EventLoop.
int	Connection::getFd() const {
	return _fd.get();
}
