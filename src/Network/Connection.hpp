#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <string>
#include <ctime>
#include "FileDescriptor.hpp"
#include "../ServerConfig/ServerConfig.hpp"

// Estado de UM cliente conectado, entre uma chamada de poll() e outra.
// Como a leitura é non-blocking, a request pode chegar em pedaços ao
// longo de várias voltas do event loop, por isso o buffer de leitura
// mora aqui no objeto e não numa variável local de função.
//
// Responsabilidade: só transporte + ciclo de vida. Mexe em BYTES
// (recv/send, buffers) e sabe quando morrer. Não sabe o que é um
// header: quem interpreta os bytes é o parser (HTTPRequest).
class Connection {
	private:
		FileDescriptor				_fd;
		std::string					_readBuffer;   // bytes recebidos até agora, ainda não processados
		std::string					_writeBuffer;  // resposta pendente de ser enviada
		const ServerConfig*			_candidate;    // Server dono deste endpoint (host:port)
		std::time_t					_lastActivity; // última vez que recebeu/enviou algo (pro timeout)

		// Único bit de estado que NÃO dá pra derivar dos buffers: se
		// está vazio o _writeBuffer, não sei distinguir "acabei, feche"
		// de "esperando a próxima request". "Lendo" vs "escrevendo" já
		// é dito por hasPendingWrite(), então não precisa de enum.
		bool						_closeRequested;

		Connection(const Connection& other);
		Connection& operator=(const Connection& other);

		// Ponto de entrega pro resto do sistema (parser -> resposta).
		// PROVISÓRIO: hoje só enfileira uma resposta fixa. Connection não
		// interpreta os bytes; quando o parser existir, é só esta linha
		// que passa o _readBuffer adiante e recebe a resposta pronta.
		void	handleRequest();

	public:
		Connection(int fd, const ServerConfig* candidate);
		~Connection();

		int			getFd() const;
		bool		hasPendingWrite() const;
		bool		isClosing() const;         // EventLoop consulta pra decidir o delete
		std::time_t	getLastActivity() const;   // EventLoop consulta pro timeout

		void	onReadable();   // recv() do que estiver disponível -> _readBuffer
		void	onWritable();   // send() do que sobrar em _writeBuffer, descontando o enviado
};

#endif
