#ifndef EVENTLOOP_HPP
# define EVENTLOOP_HPP

#include <vector>
#include <map>
#include <string>
#include <poll.h>
#include "Socket.hpp"
#include "Connection.hpp"

// Dono de tudo que é rede: os sockets ouvintes e as conexões ativas.
// É o único lugar que enxerga tanto Config (Server/Location) quanto
// Network (Socket/Connection), Config nunca inclui nada daqui.
class EventLoop {
	private:
		std::vector<ServerConfig>			_servers; // saída de Config::getServers(), copiada uma vez na construção
		std::vector<Socket*>				_listeners;	// onde ficam os sockets ouvintes
		std::vector<const ServerConfig*>	_listenerServers; // onde ficam os Servers que correspondem a cada listener
		std::map<int, Connection*>			_connections; // fd -> conexão ativa

		EventLoop(const EventLoop& other);
		EventLoop& operator=(const EventLoop& other);

		// Agrupa _servers por (host,port) único e abre UM
		// Socket ouvinte por grupo, cada socket representa um endpoint.
		void	openListeners();

		// Depois de tratar os pollfds, apaga do mapa (e delete, que
		// fecha o fd) toda Connection que se marcou como isClosing().
		void	reapClosedConnections();

		// Função para montar o pollfd todo
		std::vector<pollfd>	buildPollfds();

	public:
		explicit EventLoop(const std::vector<ServerConfig>& servers);
		~EventLoop();

		void	run();
};

#endif
