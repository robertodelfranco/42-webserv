#include "EventLoop.hpp"
#include "../Config/Color.hpp"
#include <cerrno>
#include <stdexcept>
#include <iostream>
#include <unistd.h>

#define CONNECTION_TIMEOUT 30

// Copia os Server parseados (não deveriam mudar depois disso) e já abre
// os sockets ouvintes.

/*	style guide google: "if the signature and initializer list are not
	all on one line, you must wrap before the colon and indent 4 spaces."
	sim, sou fresco rs... fica muito feio tudo numa linha. (edu) */
EventLoop::EventLoop(const std::vector<ServerConfig>& servers)
	: _servers(servers),
	  _listeners(),
	  _listenerServers(),
	  _connections() {
	openListeners();
}

// Libera os Socket* e Connection* que o próprio EventLoop criou com new
// (cada um fecha seu fd sozinho, via FileDescriptor, quando é destruído).
EventLoop::~EventLoop() {
	for (size_t i = 0; i < _listeners.size(); ++i)
		delete _listeners[i];

	for (std::map<int, Connection*>::iterator it = _connections.begin();
		it != _connections.end(); ++it)
		delete it->second;
}

void	EventLoop::openListeners() {
	for (size_t i = 0; i < _servers.size(); ++i) {
		const std::vector<Listen>& listens = _servers[i].getListens();
		for (size_t j = 0; j < listens.size(); ++j) {
			Socket* listener = new Socket();
			try {
				listener->bind(listens[j].host, listens[j].port);
				listener->listen(10); // REVER QUANTIDADE DEPOIS!!
				listener->setNonBlocking();
			} catch (...) {
				delete listener;
				throw;
			}
			_listeners.push_back(listener);
			_listenerServers.push_back(&_servers[i]);
			std::cout << Color::GREEN << "I'm listening on " << listens[j].host << " " << listens[j].port << Color::RESET << std::endl;
		}
	}
}

int	EventLoop::getPollTimeoutMs() const {
	if (_connections.empty())
		return -1;
	
	std::time_t	now = std::time(NULL);
	int	smallestRemaining = CONNECTION_TIMEOUT;

	for (std::map<int, Connection*>::const_iterator it = _connections.begin(); it != _connections.end(); ++it) {
		int elapsed = static_cast<int>(now - it->second->getLastActivity());
		int remaining = CONNECTION_TIMEOUT - elapsed;

		if (remaining <= 0 )
			return 0;
		if (remaining < smallestRemaining)
			smallestRemaining = remaining;
	}
	return smallestRemaining * 1000;
}

// POLLIN numa Connection -> ela lê mais bytes pro próprio buffer
// POLLOUT numa Connection -> ela manda o que sobrou do buffer de resposta

static pollfd	makePollfd(int fd, short events) {
	pollfd	file;
	file.fd = fd;
	file.events = events;
	file.revents = 0;
	return file;
}

std::vector<pollfd>	EventLoop::buildPollfds() {
	std::vector<pollfd>	pollfds;
	pollfds.reserve(_listeners.size() + _connections.size());
	for (size_t i = 0; i < _listeners.size(); ++i) {
		pollfds.push_back(makePollfd(_listeners[i]->getFd(), POLLIN));
	}
	for (std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it) {
		short events = 0;
		if (it->second->wantsRead())
			events |= POLLIN;
		if (it->second->hasPendingWrite())
			events |= POLLOUT;
		pollfds.push_back(makePollfd(it->first, events));
	}
	
	return pollfds;
}

void	EventLoop::run() {
	for (;;) {
		std::vector<pollfd>	pollfds = buildPollfds();
		
		if (pollfds.empty())
			continue; // não há sockets para monitorar, então não faz sentido chamar poll()
	
		int	timeoutMs = getPollTimeoutMs();

		int returnCode = poll(&pollfds[0], pollfds.size(), timeoutMs);

		if (returnCode < 0 ) {
			if (errno == EINTR)
				continue ; // poll() interrompido por sinal não é erro
			throw std::runtime_error("EventLoop: poll failed");
		}

		std::time_t now = std::time(NULL);

		for (size_t i = 0; i < _listeners.size(); ++i)
			if (pollfds[i].revents & POLLIN)
				acceptReadyListener(i);

		for (size_t i = _listeners.size(); i < pollfds.size(); ++i)
			handleConnectionEvent(pollfds[i], now);
		reapClosedConnections();
	}
}

void	EventLoop::acceptReadyListener(size_t listenerIndex) {
	for (;;) {
		int	clientFd = _listeners[listenerIndex]->accept();
		if (clientFd >= 0) {
			try {
				Connection* conn = new Connection(clientFd, _listenerServers[listenerIndex]);
				_connections[clientFd] = conn;
			} catch (const std::exception& e) {
				::close(clientFd);
				std::cerr << Color::RED << "EventLoop: failed to create connection: " << e.what() << Color::RESET << std::endl;
			}
			continue;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		if (errno == EINTR)
			continue;
		std::cerr << Color::RED << "EventLoop: accept failed" << Color::RESET << std::endl;
		return;
	}
}

void	EventLoop::handleConnectionEvent(const pollfd& event, std::time_t now) {
	std::map<int, Connection*>::iterator it = _connections.find(event.fd);
	if (it == _connections.end())
		return;

	Connection* conn = it->second;
	if (event.revents & POLLNVAL) {
		conn->requestClose();
		return;
	}
	if (now - conn->getLastActivity() >= CONNECTION_TIMEOUT)
		conn->onTimeout();
	if (conn->isClosing())
		return;
	if ((event.revents & (POLLIN | POLLHUP | POLLERR)) && conn->wantsRead())
		conn->onReadable();
	if ((event.revents & POLLOUT) && !conn->isClosing() && conn->hasPendingWrite())
		conn->onWritable();
}

void	EventLoop::reapClosedConnections() {
	std::map<int, Connection*>::iterator it = _connections.begin();
	while (it != _connections.end()) {
		if (it->second->getState() == CLOSED) {
			delete it->second;        // ~FileDescriptor fecha o fd
			_connections.erase(it++); // avança ANTES de invalidar o iterador (C++98)
		} else {
			++it;
		}
	}
}
