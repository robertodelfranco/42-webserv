#include "FileDescriptor.hpp"
#include <unistd.h>

// Só guarda o número do fd recebido, não abre nada aqui e quem cria o fd
FileDescriptor::FileDescriptor(int fd) : _fd(fd) {}

FileDescriptor::~FileDescriptor() {
	close();
}

// Só devolve o número do fd pra quem precisa passar ele numa syscall
// (bind, listen, accept, etc.) não transfere posse, só empresta.
int	FileDescriptor::get() const {
	return _fd;
}

// Fecha o fd de verdade se ainda estiver aberto (_fd >= 0).
// Depois marca _fd como -1 pra essa mesma chamada não fechar de novo
void	FileDescriptor::close() {
	if (_fd >= 0) {
		::close(_fd);
		_fd = -1;
	}
}

// Devolve o fd SEM fechar e marca este objeto como "vazio" (-1). Usado
// quando alguém de fora vai assumir a posse do fd (ex.: dup2 no CGI).
int	FileDescriptor::release() {
	int fd = _fd;
	_fd = -1;
	return fd;
}

// Aceita um fd de fora (pipe do cgi por exemplo) e deleta o fd atual
void	FileDescriptor::reset(int fd) {
	close();
	_fd = fd;
}
