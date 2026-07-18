#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <string>
#include <vector>
#include "FileDescriptor.hpp"
#include "../Config/Server.hpp"

// Estado de UM cliente conectado, entre uma chamada de poll() e outra.
// Como a leitura é non-blocking, a request pode chegar em pedaços ao
// longo de várias voltas do event loop, por isso o buffer de leitura
// mora aqui no objeto e não numa variável local de função.
class Connection {
	private:
		FileDescriptor				_fd;
		std::string					_readBuffer;  // bytes recebidos até agora, ainda não processados
		std::string					_writeBuffer; // resposta pendente de ser enviada
		std::vector<const Server*>	_candidates;  // Server(s) candidatos deste endpoint (host:port); resolvido de fato pelo header Host: quando a request chegar

		Connection(const Connection& other);
		Connection& operator=(const Connection& other);

	public:
		Connection(int fd, const std::vector<const Server*>& candidates);
		~Connection();

		int	getFd() const;

		// TODO: onReadable() -> recv() do que estiver disponível
		// agora pra _readBuffer; tentar parsear quando já tiver o
		// suficiente (fim dos headers, depois body completo).
		// TODO: onWritable() -> send() do que sobrar em
		// _writeBuffer; ir descontando o que foi enviado de fato.
		// TODO: resolveServer() -> escolher o Server certo em
		// _candidates a partir do header Host: (fallback: o primeiro).
};

#endif
