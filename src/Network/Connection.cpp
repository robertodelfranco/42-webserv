#include "Connection.hpp"
#include "Socket.hpp"
#include "Router.hpp"
#include <iostream>
#include <cerrno>
#include <cstdlib>
#include <sstream>
#include <poll.h>
#include <sys/socket.h>
#include "../Response/ErrorResponse.hpp"
#include "../Utils/Color.hpp"
#include "../Utils/Logger.hpp"
#include "../Cgi/CgiProcess.hpp"
#include "../Cgi/CgiOutputParser.hpp"

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif


// 4096 gerava syscall demais em corpo grande (recv/send de 100MB em CGI).
// Não muda a semântica, só quantos bytes por volta do poll().
static const size_t	IO_BUFFER_SIZE = 65536;

/*	EDU (AUG22): removi todas as funcoes soltas do framing provisorio.
	aquela logica foi toda transferida pro HttpParser. */

// Só guarda o fd (já aceito em outro lugar) e a lista de Server
// candidatos desse endpoint, nenhuma leitura/escrita acontece aqui.

/*	EDU (AUG22): Tirei as variáveis de controle de estado do request
	(headersEnd, bodyExpected, etc). O parser agora cuida do proprio estado */
Connection::Connection(int fd, const ServerConfig* candidate)
: _fd(fd), _readBuffer(), _keepAlive(false), _writeBuffer(),
  _writeOffset(0), _candidate(candidate), _lastActivity(std::time(NULL)),
  _parser(), _request(), _response(), _state(READING), _matchedLoc(NULL), _cgi(NULL) {
	Socket::setNonBlocking(_fd.get());
}

// delete num CgiProcess mata e colhe o filho (o destrutor dele faz isso),
// então nenhum caminho de morte da conexão deixa processo solto pra trás.
Connection::~Connection() {
	delete _cgi;
}

int	Connection::getFd() const {
	return _fd.get();
}

bool	Connection::hasPendingWrite() const {
	return _state == WRITING && _writeOffset < _writeBuffer.size();
}

bool	Connection::wantsRead() const {
	return _state == READING;
}

bool	Connection::wantsKeepAlive() const {
	return _keepAlive;
}

bool	Connection::isClosing() const {
	return _state == CLOSED;
}

std::time_t	Connection::getLastActivity() const {
	return _lastActivity;
}

void	Connection::requestClose() {
	_state = CLOSED;
}

void	Connection::onTimeout() {
	if (_state == CLOSED)
		return;
	/* CGI morre, o cliente recebe um 504 e o filho um SIGKILL. */
	if (_state == CGI_RUNNING) {
		abortCgi(504);
		return;
	}
	if (_state != READING) {
		_state = CLOSED;
		return;
	}
	buildErrorResponse(408);
	_lastActivity = std::time(NULL);
}

void	Connection::decideKeepAlive() {
	const std::string	value = toLower(_request.getHeader("connection"));

	_keepAlive = _request.getHTTPVersion() != "HTTP/1.0";
	if (value.find("close") != std::string::npos)
		_keepAlive = false;
	else if (value.find("keep-alive") != std::string::npos)
		_keepAlive = true;
}

/* Uma request inteira chegou: parseia e entrega pro handler.
Cada exceção do HttpParser vira aqui o status code traduzido,
em vez de todas caírem no mesmo 405 de antes. */
void	Connection::parseAndDispatch() {
	try {
		_parser.parse(_request);
		decideKeepAlive();
		handleRequest();
	}

	catch (const HttpParser::MethodException& e) {
		Logger::warning() << "fd=" << _fd.get() << " " << e.what();
		buildErrorResponse(405);
	}

	catch (const HttpParser::HTTPVersionException& e) {
		Logger::warning() << "fd=" << _fd.get() << " " << e.what();
		buildErrorResponse(505);
	}

	catch (const std::exception& e) {
		Logger::warning() << "fd=" << _fd.get() << " parser error: " << e.what();
		buildErrorResponse(400);
	}
}

/*	Único lugar que decide o que fazer com o retorno do feed() */
void	Connection::handleParserStatus(RequestStatus status) {
	switch (status) {
		case REQ_INCOMPLETE:
			break; // volta pro poll() e espera o resto chegar
		case REQ_COMPLETE:
			parseAndDispatch();
			break;
		case REQ_TOO_LARGE:
			Logger::warning() << "fd=" << _fd.get() << " body acima do limite de "
				<< _candidate->getBodySize() << " bytes";
			buildErrorResponse(413);
			break;
		case REQ_HEADERS_TOO_LARGE:
			Logger::warning() << "fd=" << _fd.get() << " bloco de headers acima de "
				<< MAX_HEADER_SIZE << " bytes";
			buildErrorResponse(431);
			break;
		case REQ_BAD:
			Logger::warning() << "fd=" << _fd.get() << " framing invalido";
			buildErrorResponse(400);
			break;
	}
}

void	Connection::resetForNextRequest() {
	_writeBuffer.clear();
	_writeOffset = 0;
	_request = HttpRequest();
	_response = Response();
	_matchedLoc = NULL;
	_parser = HttpParser();
	_lastActivity = std::time(NULL); // nova janela de ociosidade
	_state = READING;

	// keep-alive limpo, nenhum resto do CGI anterior atravessa pra próxima
	// request (o finishCgi/abortCgi já deletou, isso é só a garantia).
	if (_cgi) {
		delete _cgi;
		_cgi = NULL;
	}

	Logger::debug() << "fd=" << _fd.get() << " keep-alive: pronto pra proxima ("
		<< _readBuffer.size() << " bytes ja no buffer)";

	/*	EDU (AUG22): isso daqui precisa ficar porque resolvemos manter o
		pipelining. se sobrar lixo no _readBuffer (pipelining), a gente
		ja joga pro parser de novo pra garantir que a proxima request
		continue de onde parou.  */
	if (!_readBuffer.empty()) {
		std::string leftover = _readBuffer;
		_readBuffer.clear();

		handleParserStatus(_parser.feed(leftover.c_str(), leftover.size(),
										_candidate->getBodySize()));
	}
}

void	Connection::onReadable() {
	if (!wantsRead())
		return;

	char	buf[IO_BUFFER_SIZE];
	ssize_t	n = recv(_fd.get(), buf, sizeof(buf), 0);

	if (n < 0) {
		Logger::debug() << "fd=" << _fd.get() << " recv falhou, encerrando";
		_state = CLOSED;
		return;
	}
	if (n == 0) {
		Logger::debug() << "fd=" << _fd.get() << " EOF do cliente";
		_state = CLOSED;
		return;
	}

	_lastActivity = std::time(NULL);
	Logger::debug() << "fd=" << _fd.get() << " recv " << n << " bytes";

	/*	Foi embora o checkRequestFraming(), agora usamos o feed() do HttpParser
		pra saber se a request está completa. A conexao so repassa os bytes
		crus e deixa a maquina de estados decidir o RequestStatus. */
	handleParserStatus(_parser.feed(buf, n, _candidate->getBodySize()));
}

// Mesma regra do recv que só pode ler/enviar vigiado pelo poll()
void	Connection::onWritable() {
	ssize_t	n = send(_fd.get(), _writeBuffer.data() + _writeOffset,
						_writeBuffer.size() - _writeOffset, MSG_NOSIGNAL);

	if (n <= 0) {
		Logger::debug() << "fd=" << _fd.get() << " send falhou, encerrando";
		_state = CLOSED;
		return;
	}

	_writeOffset += static_cast<size_t>(n);
	_lastActivity = std::time(NULL);
	Logger::debug() << "fd=" << _fd.get() << " send " << n << " bytes, faltam "
		<< (_writeBuffer.size() - _writeOffset);

	if (_writeOffset != _writeBuffer.size())
		return; // ainda falta resposta pra mandar, o poll() chama de novo

	if (!_keepAlive) {
		_state = CLOSED;
		return;
	}

	resetForNextRequest();
}

void	Connection::handleRequest() {
	// Logger::info() << "fd=" << _fd.get() << " " << _request.getMethod() << " " << _request.getPath();
	_state = PROCESSING;

	_matchedLoc = Router::matchLocation(*_candidate, _request.getPath());

	if (!_matchedLoc) {
		buildErrorResponse(404);
		return ;
	}

	// método barrado pela config é 405, e é decidido antes de escolher handler
	if (!Router::methodAllowed(*_matchedLoc, _request)) {
		Logger::warning() << "fd=" << _fd.get() << " metodo " << _request.getMethod()
			<< " nao permitido em " << _matchedLoc->getPath();
		buildErrorResponse(405);
		return ;
	}

	IRequestHandler* handler = Router::createHandler(*_matchedLoc, _request);
	if (!handler) {
		Logger::error() << "fd=" << _fd.get() << " nenhum handler para "
			<< _request.getPath();
		buildErrorResponse(500);
		return ;
	}

	bool isDone = false;
	try {
		isDone = handler->handle(_request, *_matchedLoc, *this);
	} catch (const std::exception& e) {
		Logger::error() << "fd=" << _fd.get() << " handler lançou: " << e.what();
		delete handler;
		buildErrorResponse(500);
		return ;
	}
	delete handler;

	if (!isDone) {
		_state = CGI_RUNNING;
		return ;
	}

	// O handler disse "terminei" mas não entregou nada pelo sendResponse ou
	// queueResponse. É bug do handler, não do cliente, então erro interno
	if (_state != WRITING) {
		Logger::error() << "fd=" << _fd.get() << " handler terminou sem responder";
		buildErrorResponse(500);
	}
}

State	Connection::getState() const {
	return _state;
}

void	Connection::setState(State newState) {
	_state = newState;
}

void	Connection::queueResponse(const std::string& raw) {
	_writeBuffer = raw;
	_writeOffset = 0;
	_state = WRITING;
}

// Única saída de resposta do servidor. A Connection não decide status, 
// ela recebe o Response pronto, resolve o keep-alive e serializa.
void	Connection::sendResponse(const Response& resp) {
	_response = resp;

	// a resposta pode EXIGIR fechar (erro), mas nunca exigir manter aberto
	// isso é decisão da conexão, tomada no decideKeepAlive() a partir da request
	if (_response.getCloseAfterSend())
		_keepAlive = false;
	_response.setCloseAfterSend(!_keepAlive);

	Logger::info() << "fd=" << _fd.get() << " -> " << _response.getStatus()
		<< " " << _response.getStatusMessage() << " ("
		<< _response.getBody().size() << " bytes)";

	queueResponse(_response.toString());
}

/* Só traduz o código na resposta, quem sabe montar página de erro é o ErrorResponse. 
O _matchedLoc pode ser NULL nos erros de framing (parser) antes de casar o location */
void	Connection::buildErrorResponse(int code) {
	sendResponse(ErrorResponse(code, _candidate, _matchedLoc));
}

// ============================== CGI ==============================

void	Connection::adoptCgiProcess(CgiProcess* cgi) {
	delete _cgi; // não deveria haver outro, mas dois filhos numa conexão só seria pior
	_cgi = cgi;
}

bool	Connection::hasCgi() const {
	return _cgi != NULL;
}

CgiProcess*	Connection::getCgi() {
	return _cgi;
}

unsigned short	Connection::getServerPort() const {
	const std::vector<Listen>&	listens = _candidate->getListens();

	return listens.empty() ? 0 : listens[0].port;
}

// função pra decidir qual ponta está o evento (in ou out)
void	Connection::onCgiFdEvent(int fd, short revents) {
	if (!_cgi)
		return;

	if (fd == _cgi->getStdinFd()) {
		// POLLERR/POLLNVAL: a ponta morreu de vez, não adianta tentar escrever.
		// POLLHUP é o filho tendo fechado o stdin dele, write resolve o erro
		// e o onStdinWritable fecha a ponta
		if (revents & (POLLERR | POLLNVAL))
			_cgi->closeStdin();
		else if (revents & (POLLOUT | POLLHUP))
			_cgi->onStdinWritable();
	}
	else if (fd == _cgi->getStdoutFd()) {
		// POLLHUP no stdout é o filho terminando, ainda pode haver bytes no
		// cano, então lemos do mesmo jeito e é o read == 0 que encontra o EOF
		if (revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL))
			_cgi->onStdoutReadable();
	}
	_lastActivity = std::time(NULL);
}

void	Connection::checkCgi(std::time_t now) {
	if (!_cgi)
		return;

	// O prazo é cobrado do fork, não da última atividade porque se o script
	// escreve um byte por segundo pra sempre, ele também precisa morrer.
	if ((_cgi->getPhase() == CgiProcess::RUNNING || _cgi->getPhase() == CgiProcess::REAPING)
		&& now - _cgi->getStartTime() >= CGI_TIMEOUT) {
		Logger::warning() << "fd=" << _fd.get() << " CGI estourou " << CGI_TIMEOUT << "s";
		abortCgi(504);
		return;
	}

	if (_cgi->getPhase() == CgiProcess::REAPING)
		_cgi->checkChild();

	if (_cgi->getPhase() == CgiProcess::FAILED) {
		abortCgi(502);
		return;
	}
	if (_cgi->getPhase() == CgiProcess::COMPLETE)
		finishCgi();
}

void	Connection::finishCgi() {
	std::string	response;

	// Se falhar, as duas são 502 (o erro é do "gateway", não do cliente) ou o
	// script morreu sem dizer nada, ou escreveu algo que não tem bloco de headers.
	if (!CgiOutputParser::toHttpResponse(_cgi->getOutput(), _keepAlive, response)) {
		Logger::warning() << "fd=" << _fd.get() << " CGI devolveu saida invalida (exit="
			<< _cgi->getExitStatus() << ", " << _cgi->getOutput().size() << " bytes)";
		delete _cgi;
		_cgi = NULL;
		buildErrorResponse(502);
		return;
	}

	Logger::info() << "fd=" << _fd.get() << " CGI ok (exit=" << _cgi->getExitStatus()
		<< ", " << response.size() << " bytes)";
	delete _cgi;
	_cgi = NULL;
	queueResponse(response);
	_lastActivity = std::time(NULL);
}

/* O delete mata o filho se ele ainda estiver vivo, ~CgiProcess dá
SIGKILL e depois a conexão volta ao "normal" */
void	Connection::abortCgi(int code) {
	delete _cgi;
	_cgi = NULL;
	buildErrorResponse(code);
	_lastActivity = std::time(NULL);
}

void	Connection::dropCgi() {
	delete _cgi;
	_cgi = NULL;
}

void	Connection::onCgiClientEvent() {
	char	buf[IO_BUFFER_SIZE];
	ssize_t	n = recv(_fd.get(), buf, sizeof(buf), 0);

	/*	EDU: o buffer passa a acumular bytes lidos durante o CGI.
		se o cliente mandar request adiantada, a gente nao perde
		e processa no resetForNextRequest.*/
	if (n > 0) {
		_readBuffer.append(buf, static_cast<size_t>(n));
		_lastActivity = std::time(NULL);
		return;
	}

	/* n == 0 é o cliente que fechou e então não vai ler nenhuma resposta */
	Logger::debug() << "fd=" << _fd.get() << " cliente desistiu durante o CGI";
	dropCgi();
	_state = CLOSED;
}

int	Connection::remainingBudget(std::time_t now) const {
	if (_state == CGI_RUNNING && _cgi) {
		/* Já vi o EOF e só falta o waitpid retornar. Não há mais fd
		pra acordar o poll(), então o timeout tem que ser 0 pra a
		volta seguinte colher o filho */
		if (_cgi->getPhase() != CgiProcess::RUNNING)
			return 0;

		int	remaining = CGI_TIMEOUT - static_cast<int>(now - _cgi->getStartTime());
		return remaining < 0 ? 0 : remaining;
	}

	int	remaining = CONNECTION_TIMEOUT - static_cast<int>(now - _lastActivity);
	return remaining < 0 ? 0 : remaining;
}
