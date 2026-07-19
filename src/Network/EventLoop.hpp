#ifndef EVENTLOOP_HPP
# define EVENTLOOP_HPP

#include <vector>
#include <map>
#include <string>
#include "../Config/Server.hpp"
#include "Socket.hpp"
#include "Connection.hpp"

// Dono de tudo que é rede: os sockets ouvintes e as conexões ativas.
// É o único lugar que enxerga tanto Config (Server/Location) quanto
// Network (Socket/Connection), Config nunca inclui nada daqui.
class EventLoop {
	private:
		std::vector<Server>							_servers; // saída de Config::getServers(), copiada uma vez na construção
		std::vector<Socket*>						_listeners; // um Socket ouvinte por endpoint (host,port) único
		std::vector<std::vector<const Server*> >	_listenerCandidates; // mesmo índice de _listeners: quais Server atendem aquele endpoint
		std::map<int, Connection*>					_connections; // fd -> conexão ativa

		EventLoop(const EventLoop& other);
		EventLoop& operator=(const EventLoop& other);

		// Agrupa _servers por (host,port) único (várias server{} podem
		// compartilhar o mesmo endpoint, virtual hosting) e abre UM
		// Socket ouvinte por grupo, guardando os candidatos em paralelo.
		void	openListeners();

	public:
		explicit EventLoop(const std::vector<Server>& servers);
		~EventLoop();

		// To Do : run() -- o loop de verdade:
		// montar um std::vector<pollfd> com _listeners + _connections
		// chamar ::poll() UMA vez por volta (nunca ler/escrever sem passar por ele antes)
		// POLLIN num listener -> accept() e criar uma Connection nova (com os candidatos daquele índice)
		// POLLIN numa Connection -> ela lê mais bytes pro próprio buffer
		// POLLOUT numa Connection -> ela manda o que sobrou do buffer de resposta
		void	run();
};

#endif
