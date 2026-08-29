#ifndef EVENTLOOP_HPP
# define EVENTLOOP_HPP

#include <vector>
#include <map>
#include <string>
#include <poll.h>
#include <csignal>
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

		// Setada só pelo handler de SIGINT/SIGTERM run() checa a cada volta
		// pra sair do for(;;) por bem, deixando o destructor rodar de verdade
		// (fecha listeners e conexões) em vez do processo morrer no meio do poll().
		static volatile sig_atomic_t		_running;

		EventLoop(const EventLoop& other);
		EventLoop& operator=(const EventLoop& other);

		// Agrupa _servers por (host,port) único e abre UM
		// Socket ouvinte por listen. Listen repetido é erro de config/startup.
		void	openListeners();
		void	acceptReadyListener(size_t listenerIndex);
		void	handleConnectionEvent(const pollfd& event, std::time_t now);

		// Depois de tratar os pollfds, apaga do mapa (e delete, que
		// fecha o fd) toda Connection que se marcou como isClosing().
		void	reapClosedConnections();

		// Pega o quanto de timeout pro poll
		int		getPollTimeoutMs() const;

		/* owners[i] é a Connection dona de pollfds[i] (NULL nos slots dos
		listeners). Com os pipes do CGI no meio, "tudo depois dos listeners é
		socket de conexão" deixou de valer, e é o owners que diz de quem é
		cada fd. Ele é reconstruído do zero a cada volta justamente pra nunca
		ficar dessincronizado ou perder o pipe do cgi */
		std::vector<pollfd>	buildPollfds(std::vector<Connection*>& owners);

		/* Depois de despachar os eventos, cobra o prazo do CGI e faz o
		waitpid das conexões que já receberam o EOF. Precisa ser um passo próprio
		porque um filho travado (ou já morto) não gera evento nenhum no poll */
		void	pumpCgiConnections(std::time_t now);

	public:
		explicit EventLoop(const std::vector<ServerConfig>& servers);
		~EventLoop();

		void	run();

		// Assinatura exigida por signal(): só marca a saída, nao mexe em
		// mais nada (handler de sinal so pode tocar em sig_atomic_t volatile)
		static void	requestStop(int signum);
};

#endif
