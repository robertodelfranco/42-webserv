#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <string>
#include <ctime>
#include "FileDescriptor.hpp"
#include "../ServerConfig/ServerConfig.hpp"
#include "../Request/HttpRequest.hpp"
#include "../Request/HttpParser.hpp"
#include "../Response/HttpResponse.hpp"

/* forward declaration: o Connection só guarda um ponteiro pro processo do CGI
e delega tudo. Quem inclui Connection.hpp não precisa saber o que é um pipe. */
class CgiProcess;

// Janela de ociosidade de uma conexão comum. Uma conexão em CGI_RUNNING é
// ISENTA disso: ela não está ociosa, está esperando o filho (que tem o
// prazo próprio dele, o CGI_TIMEOUT).
#define CONNECTION_TIMEOUT 30

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
		size_t				_requestEnd;   // fim da request tratada dentro do _readBuffer (o que vier depois já é a próxima)
		bool				_chunked;      // body veio com Transfer-Encoding: chunked
		bool				_keepAlive;    // reaproveitar a conexão depois de responder
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

		/* NULL quando não há CGI rodando. O CgiHandler é deletado logo depois
		do handle(), então ele passa a posse do processo pra cá antes de morrer:
		o filho precisa de alguém que viva por várias voltas do poll(). */
		CgiProcess*			_cgi;

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
		void			decideKeepAlive();
		void			processReadBuffer();
		void			resetForNextRequest();	// limpa o estado da request anterior e volta pra READING

		// Ponto de entrega pro resto do sistema (parser -> resposta).
		// PROVISÓRIO: hoje só enfileira uma resposta fixa. Connection não
		// interpreta os bytes; quando o parser existir, é só esta linha
		// que passa o _readBuffer adiante e recebe a resposta pronta.
		void			handleRequest();

		// o CGI acabou bem: traduz a saída do script e enfileira a resposta
		void			finishCgi();
		// o CGI acabou mal (timeout, crash, saída inválida): mata e responde erro
		void			abortCgi(int code);

	public:
		Connection(int fd, const ServerConfig* candidate);
		~Connection();

		int				getFd() const;
		bool			hasPendingWrite() const;
		bool			wantsRead() const;
		bool			wantsKeepAlive() const;
		bool			isClosing() const;         // EventLoop consulta pra decidir o delete
		void			requestClose();
		void			onTimeout();
		std::time_t		getLastActivity() const;   // EventLoop consulta pro timeout

		void			onReadable();   // recv() do que estiver disponível -> _readBuffer
		void			onWritable();   // send() do que sobrar em _writeBuffer, descontando o enviado

		// é por aqui que os IRequestHandler entregam o que produziram.
		// o handleRequest() passa a chamar queueResponse(_response.toString())
		void			queueResponse(const std::string& raw);

		// os handlers respondem erro por aqui (o CGI usa pra 404/403/502/504)
		void			buildErrorResponse(int code);

		// para acessar o estado do processamento do request pelo handler.
		State			getState() const;
		void			setState(State newState);

		// ======================== CGI ========================

		/* O handler entrega o processo já lançado e some. A partir daqui a
		conexão é a dona: se ela morrer, o destructor mata o filho junto. */
		void			adoptCgiProcess(CgiProcess* cgi);
		bool			hasCgi() const;
		CgiProcess*		getCgi();

		/* Um dos pipes do CGI acordou no poll(). A Connection não lê nem
		escreve em pipe: ela só descobre de qual das duas pontas veio o evento
		e repassa pro CgiProcess. */
		void			onCgiFdEvent(int fd, short revents);

		/* Mata o filho e esquece o CGI, SEM montar resposta: é o caso do
		cliente que desistiu no meio: não há mais pra quem responder. */
		void			dropCgi();

		/* O socket do cliente acordou enquanto o CGI roda. Pode ser o cliente
		indo embora (fecha tudo) ou ele adiantando a próxima request
		(guardada pro keep-alive tratar depois). */
		void			onCgiClientEvent();

		/* Chamada uma vez por volta do loop pra toda conexão em CGI_RUNNING:
		é aqui que o timeout do filho é cobrado e que o waitpid acontece. Sem
		isso um filho travado e mudo nunca geraria evento nenhum. */
		void			checkCgi(std::time_t now);

		/* Quantos segundos faltam pro prazo DESTA conexão estourar - o do CGI
		quando há um rodando, o de ociosidade caso contrário. O EventLoop usa
		isso pro timeout do poll(). */
		int				remainingBudget(std::time_t now) const;

		// porta do listener que aceitou esta conexão (meta-variável SERVER_PORT)
		unsigned short	getServerPort() const;

	};

#endif
