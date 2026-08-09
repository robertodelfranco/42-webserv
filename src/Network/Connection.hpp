#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <string>
#include <ctime>
#include "FileDescriptor.hpp"
#include "../ServerConfig/ServerConfig.hpp"
#include "../Request/HttpRequest.hpp"
#include "../Request/HttpParser.hpp"
#include "../Response/HttpResponse.hpp"


/* ESSE ENUM VAMOS USAR PARA REPRESENTAR O ESTADO DA CONEXÃO. */
enum State {
	READING,
	PROCESSING,
	CGI_RUNNING,
	WRITING,
	CLOSED
};

// Estado de UM cliente conectado, entre uma chamada de poll() e outra.
// Como a leitura é non-blocking, a request pode chegar em pedaços ao
// longo de várias voltas do event loop, por isso o buffer de leitura
// mora aqui no objeto e não numa variável local de função.

// Responsabilidade: só transporte + ciclo de vida. Mexe em BYTES
// (recv/send, buffers) e sabe quando morrer. Não sabe o que é um
// header: quem interpreta os bytes é o parser (HTTPRequest).
class Connection {
	private:
		FileDescriptor		_fd;
		std::string			_readBuffer;   // bytes recebidos até agora, ainda não processados
		size_t				_headersEnd;   // npos enquanto o \r\n\r\n não chegou; depois, índice do 1º byte do body
		long long			_bodyExpected; // Content-Length anunciado; -1 = ainda desconhecido
		bool				_chunked;      // body veio com Transfer-Encoding: chunked
		std::string			_writeBuffer;  // resposta pendente de ser enviada
		size_t				_writeOffset; // marca quantos bytes já foram enviados
		const ServerConfig*	_candidate;    // Server dono deste endpoint (host:port)
		std::time_t			_lastActivity; // última vez que recebeu/enviou algo (pro timeout)
		HttpParser			_parser;

		/* o Connection cria o HttpRequest e o HttpResponse. Quem vai preencher
		o HttpRequest é o HttpParser, e quem vai preencher o HttpResponse vai
		ser o IRequestHandler. O importante é que ambos são criados e destruídos
		no escopo de Connection. eu só não sei ainda se é pra instanciar o 
		objeto ou um ponteiro, aí tem a ver com o design. se ficar como objeto,
		eles precisam ter o método clear() para o keep-alive funcionar. (edu) */
		HttpRequest			_request;      // request JÁ parseada
		HttpResponse		_response;     // resposta a ser enviada
		State				_state;

		Connection(const Connection& other);
		Connection& operator=(const Connection& other);

		// "os bytes que tenho no _readBuffer já formam uma request inteira?"
		// só o Content-Length diz onde parar de ler.
		enum RequestStatus {
			REQ_INCOMPLETE,			// faltam bytes, volta pro poll()
			REQ_COMPLETE,			// mensagem inteira no buffer
			REQ_BAD,				// framing impossível de interpretar -> 400
			REQ_TOO_LARGE,			// body acima do client_max_body_size -> 413
			REQ_HEADERS_TOO_LARGE	// bloco de headers sem fim à vista -> 431
		};

		RequestStatus	checkRequestFraming();

		// Ponto de entrega pro resto do sistema (parser -> resposta).
		// PROVISÓRIO: hoje só enfileira uma resposta fixa. Connection não
		// interpreta os bytes; quando o parser existir, é só esta linha
		// que passa o _readBuffer adiante e recebe a resposta pronta.
		void			handleRequest();
		void			buildErrorResponse(int code);

	public:
		Connection(int fd, const ServerConfig* candidate);
		~Connection();

		int				getFd() const;
		bool			hasPendingWrite() const;
		bool			wantsRead() const;
		bool			isClosing() const;         // EventLoop consulta pra decidir o delete
		void			requestClose();
		void			onTimeout();
		std::time_t		getLastActivity() const;   // EventLoop consulta pro timeout

		void			onReadable();   // recv() do que estiver disponível -> _readBuffer
		void			onWritable();   // send() do que sobrar em _writeBuffer, descontando o enviado

		// é por aqui que os IRequestHandler entregam o que produziram.
		// o handleRequest() passa a chamar queueResponse(_response.toString())
		void			queueResponse(const std::string& raw);

		// para acessar o estado do processamento do request pelo handler.
		State			getState() const;
		void			setState(State newState);

	};

#endif
