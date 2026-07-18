#include "EventLoop.hpp"

// Copia os Server parseados (não deveriam mudar depois disso) e já abre
// os sockets ouvintes -- depois do construtor, o EventLoop está pronto
// pra rodar run().
EventLoop::EventLoop(const std::vector<Server>& servers)
: _servers(servers), _listeners(), _listenerCandidates(), _connections() {
	openListeners();
}

// Libera os Socket* e Connection* que o próprio EventLoop criou com new
// (cada um fecha seu fd sozinho, via FileDescriptor, quando é destruído).
EventLoop::~EventLoop() {
	for (size_t i = 0; i < _listeners.size(); ++i)
		delete _listeners[i];

	for (std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it)
		delete it->second;
}

void	EventLoop::openListeners() {
	// TODO: implementar o agrupamento + abertura dos sockets.
	//
	// A ideia: várias server{} do config podem escutar no mesmo (host,
	// port) -- é o esquema de virtual hosting (nginx faz igual). Então
	// em vez de abrir um Socket por Server, você quer agrupar por
	// endpoint único primeiro:
	//
	// 1. Percorra _servers; pra cada Server, percorra server.getListens().
	// 2. Use algo como std::map<std::pair<std::string, unsigned short>,
	//    std::vector<const Server*> > pra juntar, por (host, port), quais
	//    Server* (ponteiro pro elemento dentro de _servers) atendem
	//    aquele endpoint.
	// 3. Pra cada grupo único do map: `new Socket()`, `bind(host, port)`,
	//    `listen(backlog)`, `setNonBlocking()`, e guarde o Socket* em
	//    _listeners.
	// 4. Guarde a lista de Server* daquele grupo em _listenerCandidates,
	//    no MESMO índice do Socket* correspondente em _listeners --
	//    assim, quando run() aceitar uma conexão em _listeners[i], já
	//    sabe passar _listenerCandidates[i] pro Connection novo.
	//
	// Cuidado: guardar &_servers[i] só é seguro enquanto _servers não for
	// mais modificado depois do construtor (nada de push_back depois
	// daqui, ou os ponteiros ficam inválidos se o vector realocar).
}

void	EventLoop::run() {
	// TODO: implementar o loop de poll() -- ver os passos no .hpp
}
