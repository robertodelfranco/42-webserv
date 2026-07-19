#include "Socket.hpp"
#include <stdexcept>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// Função para construir o socket e configurá-lo.
static int	createTcpSocket() {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		throw std::runtime_error("Socket: failed to create socket");

	int opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		::close(fd); // ainda não existe FileDescriptor pra fazer isso por nós
		throw std::runtime_error("Socket: setsockopt(SO_REUSEADDR) failed");
	}
	return fd;
}

// Cria um fd de socket TCP novo e entrega pro FileDescriptor cuidar dele.
Socket::Socket() : _fd(createTcpSocket()) {}

// Embrulha um fd que já existe (ex.: já veio de accept() em outro lugar).
Socket::Socket(int fd) : _fd(fd) {}

// Corpo vazio de propósito pois o FileDescriptor já lida com o close do fd.
Socket::~Socket() {}

// Só repassa o número do fd pra quem precisa (ex.: montar um pollfd).
int	Socket::getFd() const {
	return _fd.get();
}

// Fecha o fd antes da hora, sem esperar o destrutor.
void	Socket::close() {
	_fd.close();
}

// Monta um sockaddr_in a partir de host/port e faz bind() nele. Host
// vazio ou "0.0.0.0" vira INADDR_ANY (escuta em todas as interfaces).
void	Socket::bind(const std::string& host, unsigned short port) {
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (host.empty() || host == "0.0.0.0")
		addr.sin_addr.s_addr = INADDR_ANY;
	else if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
		throw std::runtime_error("Socket: invalid host '" + host + "'");

	if (::bind(_fd.get(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
		std::ostringstream oss;
		oss << "Socket: bind failed on " << (host.empty() ? "0.0.0.0" : host) << ":" << port;
		throw std::runtime_error(oss.str());
	}
}

// Marca o fd como socket ouvinte, com a fila de conexões pendentes
// limitada a `backlog`.
void	Socket::listen(int backlog) {
	if (::listen(_fd.get(), backlog) < 0) {
		throw std::runtime_error("Socket: listen failed");
	}
}

// Aceita UMA conexão pendente e devolve o fd dela. Quem chama decide o
// que fazer (ex.: embrulhar num Socket/Connection novo).
int	Socket::accept() const {
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	int client_fd = ::accept(_fd.get(), reinterpret_cast<struct sockaddr*>(&addr), &addrlen);
	if (client_fd < 0) {
		throw std::runtime_error("Socket: accept failed");
	}
	return client_fd;
}

// Deixa o fd non-blocking, pra recv()/accept() nunca travarem esperando
// dado que ainda não chegou, essencial pro loop de poll() não parar.
void	Socket::setNonBlocking() {
	if (fcntl(_fd.get(), F_SETFL, O_NONBLOCK) < 0) {
		throw std::runtime_error("Socket: fcntl(O_NONBLOCK) failed");
	}
}
