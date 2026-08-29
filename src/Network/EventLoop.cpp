#include "../Utils/Logger.hpp"
#include "EventLoop.hpp"
#include <cerrno>
#include <string.h>
#include <stdexcept>
#include <iostream>
#include <unistd.h>
#include "../Cgi/CgiProcess.hpp"

volatile sig_atomic_t	EventLoop::_running = 1;

void	EventLoop::requestStop(int signum) {
	(void)signum;
	_running = 0;
}

EventLoop::EventLoop(const std::vector<ServerConfig>& servers)
	: _servers(servers),
	  _listeners(),
	  _listenerServers(),
	  _connections() {
	openListeners();
	if (_listeners.empty())
		throw std::runtime_error("LISTENERS EMPTY");
}

// Libera os Socket* e Connection* que o próprio EventLoop criou com new
// (cada um fecha seu fd sozinho, via FileDescriptor, quando é destruído).
EventLoop::~EventLoop() {
	Logger::info() << "Encerrando: " << _connections.size() << " conexao(oes) aberta(s)";
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
				listener->listen(128); // REVER QUANTIDADE DEPOIS!!
				listener->setNonBlocking();
			} catch (...) {
				Logger::error() << "bind/listen em " << listens[j].host << ":" << listens[j].port << " falhou";
				delete listener;
				throw;
			}
			_listeners.push_back(listener);
			_listenerServers.push_back(&_servers[i]);
			Logger::info() << "Listening on " << listens[j].host << ":" << listens[j].port 
							<< " (fd " << listener->getFd() << ")";

		}
	}
	Logger::info() << "webserv pronto: " << _listeners.size() << " listener(s) ativo(s)";
}

int	EventLoop::getPollTimeoutMs() const {
	if (_connections.empty())
		return -1;

	std::time_t	now = std::time(NULL);

	int	smallestRemaining = -1;

	for (std::map<int, Connection*>::const_iterator it = _connections.begin(); it != _connections.end(); ++it) {
		int remaining = it->second->remainingBudget(now);

		if (remaining <= 0 )
			return 0;
		if (smallestRemaining < 0 || remaining < smallestRemaining)
			smallestRemaining = remaining;
	}

	if (smallestRemaining < 0)
		return -1; // nenhuma conexão com prazo, dorme até chegar evento
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

std::vector<pollfd>	EventLoop::buildPollfds(std::vector<Connection*>& owners) {
	std::vector<pollfd>	pollfds;

	owners.clear();
	pollfds.reserve(_listeners.size() + _connections.size());
	for (size_t i = 0; i < _listeners.size(); ++i) {
		pollfds.push_back(makePollfd(_listeners[i]->getFd(), POLLIN));
		owners.push_back(NULL); // listener não pertence a conexão nenhuma
	}
	for (std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it) {
		Connection*	conn = it->second;
		short		events = 0;

		if (conn->wantsRead())
			events |= POLLIN;
		if (conn->hasPendingWrite())
			events |= POLLOUT;
		/* Sem pedir POLLIN aqui, o servidor só descobriria o abandono no timeout de 10s,
		gerando uma resposta que ninguém mais vai ler */
		if (conn->getState() == CGI_RUNNING)
			events |= POLLIN;
		pollfds.push_back(makePollfd(it->first, events));
		owners.push_back(conn);

		/* Os pipes do CGI entram no MESMO poll() dos sockets, é uma das exigências
		do subject (nenhum I/O fora do poll) e é o que evita o deadlock */
		if (conn->getState() == CGI_RUNNING && conn->hasCgi()) {
			CgiProcess*	cgi = conn->getCgi();

			if (cgi->wantsWriteInput()) {
				pollfds.push_back(makePollfd(cgi->getStdinFd(), POLLOUT));
				owners.push_back(conn);
			}
			if (cgi->wantsReadOutput()) {
				pollfds.push_back(makePollfd(cgi->getStdoutFd(), POLLIN));
				owners.push_back(conn);
			}
		}
	}

	return pollfds;
}

void	EventLoop::run() {
	while (_running) {
		std::vector<Connection*>	owners;
		std::vector<pollfd>			pollfds = buildPollfds(owners);

		int	timeoutMs = getPollTimeoutMs();

		int returnCode = poll(&pollfds[0], pollfds.size(), timeoutMs);
		Logger::debug() << "poll(): " << returnCode << " fds prontos de "
                << pollfds.size() << " (timeout " << timeoutMs << "ms)";

		if (returnCode < 0 ) {
			if (errno == EINTR)
				continue ; // poll() interrompido por sinal não é erro
			throw std::runtime_error("EventLoop: poll failed");
		}

		std::time_t now = std::time(NULL);

		for (size_t i = 0; i < _listeners.size(); ++i)
			if (pollfds[i].revents & POLLIN)
				acceptReadyListener(i);

		/* Cada slot pode ser o socket da conexão ou um dos pipes do CGI dela,
		quem diz pra mim é o owner. O isClosing protege o caso de a conexão ter
		fechado um slot antes dessa volta, os seguintes dela apontariam pra um
		objeto que já vai ser deletado */
		for (size_t i = _listeners.size(); i < pollfds.size(); ++i) {
			Connection*	conn = owners[i];

			if (!conn || conn->isClosing())
				continue;
			if (pollfds[i].fd == conn->getFd())
				handleConnectionEvent(pollfds[i], now);
			else if (pollfds[i].revents)
				conn->onCgiFdEvent(pollfds[i].fd, pollfds[i].revents);
		}

		pumpCgiConnections(now);
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
				Logger::blank(Logger::INFO); // abre o bloco desta conexao
				Logger::info() << "Nova conexao fd=" << clientFd
               		<< " no listener " << listenerIndex;
			} catch (const std::exception& e) {
				::close(clientFd);
				Logger::error() << "failed to create connection: " << e.what();
			}
			continue;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		if (errno == EINTR)
			continue;
		Logger::error() << "accept failed: " << strerror(errno);
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

	/* Algo chegou no socket com um CGI rodando, cliente desistiu ou ele 
	encaminhou a próxima request. Só o recv da conn distingue os dois */
	if (conn->getState() == CGI_RUNNING) {
		if (event.revents & (POLLIN | POLLERR | POLLHUP))
			conn->onCgiClientEvent();
		if (conn->isClosing())
			return;
	}

	/* Conexão em CGI é ISENTA do timeout de ociosidade pq ela não está parada,
	está esperando o filho que tem o prazo próprio cobrado no checkCgi() */
	if (conn->getState() != CGI_RUNNING
		&& now - conn->getLastActivity() >= CONNECTION_TIMEOUT)
		conn->onTimeout();
	if (conn->isClosing())
		return;
	if ((event.revents & (POLLIN | POLLHUP | POLLERR)) && conn->wantsRead())
		conn->onReadable();
	if ((event.revents & POLLOUT) && !conn->isClosing() && conn->hasPendingWrite())
		conn->onWritable();
}

/* checar o EOF antes do stdout fechar na próxima volta, nenhum fd do CGI está
no poll pra acordar o loop. Por isso o waitpid e a cobrança do prazo vem
depois do despacho dos eventos, fechando o ciclo na mesma passada */
void	EventLoop::pumpCgiConnections(std::time_t now) {
	for (std::map<int, Connection*>::iterator it = _connections.begin();
		it != _connections.end(); ++it) {
		if (it->second->getState() == CGI_RUNNING && !it->second->isClosing())
			it->second->checkCgi(now);
	}
}

void	EventLoop::reapClosedConnections() {
	std::map<int, Connection*>::iterator it = _connections.begin();
	while (it != _connections.end()) {
		if (it->second->getState() == CLOSED) {
			Logger::debug() << "Fechando fd=" << it->first;
			delete it->second;        // ~FileDescriptor fecha o fd
			_connections.erase(it++); // avança ANTES de invalidar o iterador (C++98)
			Logger::blank(Logger::DEBUG); // fecha o bloco desta conexao
		} else {
			++it;
		}
	}
}
