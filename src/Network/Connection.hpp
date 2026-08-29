#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <string>
#include <ctime>
#include "FileDescriptor.hpp"
#include "../ServerConfig/ServerConfig.hpp"
#include "../Request/HttpRequest.hpp"
#include "../Request/HttpParser.hpp"
#include "../Response/Response.hpp"

class CgiProcess;

// Uma conexão em CGI_RUNNING é ISENTA disso pq ela não 
// está ociosa, está esperando o filho (que tem o
// prazo próprio dele, CGI_TIMEOUT)
#define CONNECTION_TIMEOUT 90

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
		bool				_keepAlive;    // reaproveitar a conexão depois de responder
		std::string			_writeBuffer;  // resposta pendente de ser enviada
		size_t				_writeOffset; // marca quantos bytes já foram enviados
		const ServerConfig*	_candidate;    // Server dono deste endpoint (host:port)
		std::time_t			_lastActivity; // última vez que recebeu/enviou algo (pro timeout)
		HttpParser			_parser;

		/* o Connection cria o HttpRequest e o HttpResponse. Quem vai preencher
		o HttpRequest é o HttpParser, e quem vai preencher o HttpResponse vai
		ser o IRequestHandler. O importante é que ambos são criados e destruídos
		objeto ou um ponteiro, aí tem a ver com o design. se ficar como objeto,
		no escopo de Connection. eu só não sei ainda se é pra instanciar o 
		eles precisam ter o método clear() para o keep-alive funcionar. (edu) */
		HttpRequest			_request;      // request JÁ parseada
		Response			_response;     // última resposta entregue ao sendResponse
		State				_state;

		/* Location casado desta request, NULL fora do ciclo de uma request, agora é
		guardado aqui pq buildErrorResponse precisa dele pra achar o error_page certo. */
		const Location*		_matchedLoc;

		/* NULL quando não há CGI rodando. O CgiHandler é deletado logo depois
		do handle(), então ele passa a posse do processo pra cá antes de morrer,
		o filho precisa de alguém que viva por várias voltas do poll() */
		CgiProcess*			_cgi;

		Connection(const Connection& other);
		Connection& operator=(const Connection& other);

		void			decideKeepAlive();
		void			resetForNextRequest();	// limpa o estado da request anterior e volta pra READING

		/* o que fazer com cada RequestStatus devolvido pelo feed(): os dois
		pontos que alimentam o parser (onReadable e o resto de pipelining do
		resetForNextRequest) passam por aqui em vez de repetir o switch. */
		void			handleParserStatus(RequestStatus status);

		/* parse() + entrega pro handler. Cada exceção do HttpParser vira
		aqui o status code que ela merece. */
		void			parseAndDispatch();

		// Ponto de entrega pro resto do sistema (parser -> resposta).
		// PROVISÓRIO hoje só prepara uma resposta fixa. Connection não
		// interpreta os bytes. Quando o parser existir, é só esta linha
		// que passa o _readBuffer adiante e recebe a resposta pronta.
		void			handleRequest();

		// o CGI acabou bem, então traduz a saída do script e prepara a resposta
		void			finishCgi();
		// o CGI acabou mal (timeout, crash, saída inválida), mata e responde erro
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

		/* é por aqui que os IRequestHandler entregam o que produziram.
		A Connection não monta nada, só serializa e decide o keep-alive
		(a resposta pode exigir fechar via setCloseAfterSend) */
		void			sendResponse(const Response& resp);

		/* Só o CGI usa, porque a saída do script já vem pronta do
		CgiOutputParser e passar por Response perderia headers repetidos
		Quando HttpResponse aceitar headers multi-valor, o CGI migra pro
		sendResponse e isso some */
		void			queueResponse(const std::string& raw);

		/* Erro por código, quem monta a página agora é o ErrorResponse */
		void			buildErrorResponse(int code);

		// para acessar o estado do request pelo handler
		State			getState() const;
		void			setState(State newState);

		// ======================== CGI ========================

		/* O handler entrega o processo já lançado e some. A partir daqui a
		conexão é a dona, logo se ela morrer, o destrutor mata o filho junto */
		void			adoptCgiProcess(CgiProcess* cgi);
		bool			hasCgi() const;
		CgiProcess*		getCgi();

		/* Um dos pipes do CGI acordou no poll. A Connection só descobre 
		de qual das duas pontas veio o evento e repassa pro CgiProcess */
		void			onCgiFdEvent(int fd, short revents);

		/* Mata o filho e esquece o CGI, cliente desistiu,
		então não há mais pra quem responder */
		void			dropCgi();

		/* O socket do cliente acordou enquanto o CGI roda. Pode ser o cliente
		indo embora (fecha tudo) ou ele adiantando a próxima request
		(guardada ppro keep-alive tratar depois) */
		void			onCgiClientEvent();

		/* Chamada uma vez por volta do loop pra toda conexão em CGI_RUNNING.
		É aqui que o timeout do filho é cobrado e que o waitpid acontece */
		void			checkCgi(std::time_t now);

		/* Quantos segundos faltam pro prazo dessa conexão estourar, o do CGI
		quando ta rodando ou o de ociosidade, EventLoop usa isso pro timeout do poll */
		int				remainingBudget(std::time_t now) const;

		// porta do listener que aceitou esta conexão
		unsigned short	getServerPort() const;

	};

#endif
