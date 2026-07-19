#include "FileDescriptor.hpp"
#include <unistd.h>

// Só guarda o número do fd recebido, não abre nada aqui e quem cria o fd
// de verdade (socket(), pipe(), etc.) é sempre quem chama.
FileDescriptor::FileDescriptor(int fd) : _fd(fd) {}

// Roda automaticamente quando o objeto sai de escopo (RAII). Chama close()
// pra garantir que o fd é liberado mesmo se ninguém fechou manualmente.
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
// evita double-close se close() for chamado duas vezes.
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
