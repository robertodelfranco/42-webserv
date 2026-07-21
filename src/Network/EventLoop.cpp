#include "EventLoop.hpp"
#include <poll.h>
#include <cerrno>
#include <stdexcept>
#include <iostream>

// Copia os Server parseados (não deveriam mudar depois disso) e já abre
// os sockets ouvintes.

/*	style guide google: "if the signature and initializer list are not
	all on one line, you must wrap before the colon and indent 4 spaces."
	sim, sou fresco rs... fica muito feio tudo numa linha. (edu) */
EventLoop::EventLoop(const std::vector<Server>& servers)
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
			_listeners.push_back(listener); // guarda o Socket* do listener
			_listenerServers.push_back(&_servers[i]); // guarda o Server* candidato (mesmo índice de _listeners)
		}
	}
}

// montar um std::vector<pollfd> com _listeners + _connections
// chamar ::poll() UMA vez por volta (nunca ler/escrever sem passar por ele antes)
// POLLIN num listener -> accept() e criar uma Connection nova (com os candidatos daquele índice)
// POLLIN numa Connection -> ela lê mais bytes pro próprio buffer
// POLLOUT numa Connection -> ela manda o que sobrou do buffer de resposta
void	EventLoop::run() {
	for (;;) {
		std::vector<pollfd>	pollfds;
		pollfds.reserve(_listeners.size() + _connections.size());
		for (size_t i = 0; i < _listeners.size(); ++i) {
			pollfd	fd;
			fd.fd = _listeners[i]->getFd();
			fd.events = POLLIN;
			fd.revents = 0;
			pollfds.push_back(fd);
		}
		for (std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it) {
			pollfd fd;
			fd.fd = it->first;
			fd.events = POLLIN;
			if (it->second->hasPendingWrite())
				fd.events |= POLLOUT; // só pede POLLOUT quando há algo pra mandar, senão poll() nunca bloqueia
			fd.revents = 0;
			pollfds.push_back(fd);
		}

		if (pollfds.empty())
			continue; // não há sockets para monitorar, então não faz sentido chamar poll()

		int returnCode = poll(&pollfds[0], pollfds.size(), -1);

		if (returnCode < 0 ) {
			if (errno == EINTR)
				continue ; // poll() interrompido por sinal não é erro
			throw std::runtime_error("EventLoop: poll failed");
		} else if (returnCode == 0) {
			// comparar com o timeout passado e decidir se fecha conexões inativas. (add time-t em con)
		} else {
			for (size_t i = 0; i < pollfds.size(); ++i) {
				if (i < _listeners.size()) {
					if (pollfds[i].revents & POLLIN) {
						try {
							int clientFd = _listeners[i]->accept();
							Connection* conn = new Connection(clientFd, _listenerServers[i]);
							_connections[clientFd] = conn;
						} catch (const std::exception& e) {
							std::cerr << "EventLoop: accept failed: " << e.what() << "\n";
						}
					}
				} else {
					if (pollfds[i].revents & POLLIN) {
						// To do: onReadable() -> recv(). POLLHUP e POLLERR entram aqui
					}
					if (pollfds[i].revents & POLLOUT) {
						// To do: onWritable() -> send().
					}
				}
			}
		}
	}
}
