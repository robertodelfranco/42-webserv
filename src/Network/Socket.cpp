#include "Socket.hpp"
#include <stdexcept>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>     // getaddrinfo/freeaddrinfo, no lugar do inet_pton
#include "../Utils/Logger.hpp"

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
	Logger::debug() << "socket() criado fd=" << fd << " (SO_REUSEADDR on)";
	return fd;
}

// Cria um fd de socket TCP novo e entrega pro FileDescriptor cuidar dele.
Socket::Socket() : _fd(createTcpSocket()) {}

// Embrulha um fd que já existe (ex.: já veio de accept() em outro lugar).
Socket::Socket(int fd) : _fd(fd) {}

Socket::~Socket() {}

int	Socket::getFd() const {
	return _fd.get();
}

void	Socket::close() {
	_fd.close();
}

void	Socket::bind(const std::string& host, unsigned short port) {
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (host.empty() || host == "0.0.0.0")
		addr.sin_addr.s_addr = INADDR_ANY;
	else {
		struct addrinfo		hints;
		struct addrinfo*	res = NULL;

		std::memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_NUMERICHOST;

		if (getaddrinfo(host.c_str(), NULL, &hints, &res) != 0 || res == NULL)
			throw std::runtime_error("Socket: invalid host '" + host + "'");

		addr.sin_addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr)->sin_addr;
		freeaddrinfo(res);
	}

	if (::bind(_fd.get(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
		std::ostringstream oss;
		oss << "Socket: bind failed on " << (host.empty() ? "0.0.0.0" : host) << ":" << port;
		throw std::runtime_error(oss.str());
	}
	Logger::debug() << "bind() ok fd=" << _fd.get() << " em "
                << (host.empty() ? "0.0.0.0" : host) << ":" << port;
}

void	Socket::listen(int backlog) {
	if (::listen(_fd.get(), backlog) < 0) {
		throw std::runtime_error("Socket: listen failed");
	}
	Logger::debug() << "listen() ok fd=" << _fd.get() << " backlog=" << backlog;
}

int	Socket::accept() const {
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	int client_fd = ::accept(_fd.get(), reinterpret_cast<struct sockaddr*>(&addr), &addrlen);

	return client_fd;
}

void	Socket::setNonBlocking() {
	setNonBlocking(_fd.get());
	Logger::debug() << "O_NONBLOCK setado em fd= " << _fd.get();
}

void	Socket::setCloexec(int fd) {
	if(fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
		throw std::runtime_error("Socket: fcntl(F_SETFD, FD_CLOEXEC) failed");
	}
}

void	Socket::setNonBlocking(int fd) {
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
		throw std::runtime_error("Socket: fcntl(F_SETFL, O_NONBLOCK) failed");
	setCloexec(fd);
}
