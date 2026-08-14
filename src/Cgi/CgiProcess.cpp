#include "CgiProcess.hpp"
#include "../Network/Socket.hpp"
#include "../Utils/Logger.hpp"
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

CgiProcess::CgiProcess(const std::string& interpreter, const std::string& scriptPath,
					   const std::string& workDir, const std::vector<std::string>& env,
					   const std::string& body)
	: _interpreter(interpreter),
	  _scriptPath(scriptPath),
	  _workDir(workDir),
	  _env(env),
	  _pid(-1),
	  _stdinFd(),
	  _stdoutFd(),
	  _input(body),
	  _inputOffset(0),
	  _output(),
	  _startTime(std::time(NULL)),
	  _phase(RUNNING),
	  _exitStatus(0) {}

/* A rede de segurança do desenho todo: não importa por onde a conexão morreu
(cliente sumiu, exceção, servidor encerrando), o filho não fica solto e não
vira zumbi. Os dois FileDescriptor fecham os pipes sozinhos. */
CgiProcess::~CgiProcess() {
	if (_pid > 0)
		killChild();
}

// =============================== lançamento ===============================

/* Só o filho executa isto e ele NUNCA volta: ou vira o interpretador no
execve, ou sai com _exit(127). Nada de exit(): o filho é um clone do webserv
inteiro, e exit() rodaria os destructors e o flush do Logger do clone, cuspindo
lixo no socket de um cliente que nem é dele. */
void	CgiProcess::runChild(int inPipe[2], int outPipe[2]) {
	if (dup2(inPipe[0], STDIN_FILENO) < 0 || dup2(outPipe[1], STDOUT_FILENO) < 0)
		_exit(127);

	// as 4 pontas originais já estão duplicadas em 0 e 1, não servem mais.
	// (o teste protege o caso improvável de o pipe ter caído em cima do 0/1)
	if (inPipe[0] > STDERR_FILENO)
		::close(inPipe[0]);
	if (inPipe[1] > STDERR_FILENO)
		::close(inPipe[1]);
	if (outPipe[0] > STDERR_FILENO)
		::close(outPipe[0]);
	if (outPipe[1] > STDERR_FILENO)
		::close(outPipe[1]);

	// o script espera rodar de dentro da própria pasta (open("dados.txt"))
	if (!_workDir.empty() && chdir(_workDir.c_str()) < 0)
		_exit(127);

	/* argv/envp: as strings vivem em _interpreter/_scriptPath/_env, que são
	membros e continuam vivos até o execve. Os vetores abaixo só colhem
	ponteiros pra elas, e nada é inserido em _env depois desta linha (um
	push_back aqui realocaria e deixaria todos os ponteiros pendurados). */
	std::vector<char*>	argv;
	argv.push_back(const_cast<char*>(_interpreter.c_str()));
	argv.push_back(const_cast<char*>(_scriptPath.c_str()));
	argv.push_back(NULL);

	std::vector<char*>	envp;
	for (size_t i = 0; i < _env.size(); ++i)
		envp.push_back(const_cast<char*>(_env[i].c_str()));
	envp.push_back(NULL);

	execve(_interpreter.c_str(), &argv[0], &envp[0]);
	_exit(127); // se voltou, o execve falhou
}

bool	CgiProcess::start() {
	int	inPipe[2];
	int	outPipe[2];

	if (pipe(inPipe) < 0) {
		Logger::error() << "CGI: pipe() de entrada falhou";
		_phase = FAILED;
		return false;
	}
	if (pipe(outPipe) < 0) {
		Logger::error() << "CGI: pipe() de saida falhou";
		::close(inPipe[0]);
		::close(inPipe[1]);
		_phase = FAILED;
		return false;
	}

	_pid = fork();
	if (_pid < 0) {
		Logger::error() << "CGI: fork() falhou";
		::close(inPipe[0]);
		::close(inPipe[1]);
		::close(outPipe[0]);
		::close(outPipe[1]);
		_phase = FAILED;
		return false;
	}
	if (_pid == 0)
		runChild(inPipe, outPipe); // não volta

	/* Fechar as pontas do filho não é higiene, é obrigação: o kernel só manda
	EOF no cano de saída quando TODO mundo que podia escrever nele fechou. Se
	outPipe[1] continuar aberto aqui no pai, o EOF nunca chega e todo CGI
	"trava" até o timeout. */
	::close(inPipe[0]);
	::close(outPipe[1]);

	_stdinFd.reset(inPipe[1]);
	_stdoutFd.reset(outPipe[0]);

	try {
		// O_NONBLOCK porque o loop nunca pode bloquear; FD_CLOEXEC (que o
		// setNonBlocking já aplica) pra que o PRÓXIMO CGI não herde estes canos.
		Socket::setNonBlocking(_stdinFd.get());
		Socket::setNonBlocking(_stdoutFd.get());
	} catch (const std::exception& e) {
		Logger::error() << "CGI: " << e.what();
		killChild();
		_phase = FAILED;
		return false;
	}

	// GET e afins: sem body, o EOF do stdin tem que ser imediato, senão o
	// script fica esperando pra sempre no sys.stdin.read().
	if (_input.empty())
		_stdinFd.close();

	_startTime = std::time(NULL);
	_phase = RUNNING;
	Logger::debug() << "CGI: pid=" << _pid << " stdin=" << _stdinFd.get()
		<< " stdout=" << _stdoutFd.get() << " body=" << _input.size() << " bytes";
	return true;
}

// ================================ estado ================================

bool	CgiProcess::wantsWriteInput() const {
	return _stdinFd.get() >= 0 && _inputOffset < _input.size();
}

bool	CgiProcess::wantsReadOutput() const {
	return _stdoutFd.get() >= 0;
}

int	CgiProcess::getStdinFd() const {
	return _stdinFd.get();
}

int	CgiProcess::getStdoutFd() const {
	return _stdoutFd.get();
}

CgiProcess::Phase	CgiProcess::getPhase() const {
	return _phase;
}

int	CgiProcess::getExitStatus() const {
	return _exitStatus;
}

const std::string&	CgiProcess::getOutput() const {
	return _output;
}

std::time_t	CgiProcess::getStartTime() const {
	return _startTime;
}

// ================================== I/O ==================================

void	CgiProcess::closeStdin() {
	_stdinFd.close();
}

/* Um write por evento, e o close do fim é o que o script está esperando:
é o fechar da ponta de escrita que produz o EOF no sys.stdin.read() dele. */
void	CgiProcess::onStdinWritable() {
	if (_stdinFd.get() < 0 || _inputOffset >= _input.size())
		return;

	ssize_t	n = write(_stdinFd.get(), _input.data() + _inputOffset,
					  _input.size() - _inputOffset);

	if (n <= 0) {
		/* O filho fechou o stdin dele antes de ler tudo (EPIPE). Isso é normal
		- um script pode ignorar o body - e não é fatal: fecha esta ponta e
		segue lendo a saída, que é o que interessa. Nada de olhar errno. */
		Logger::debug() << "CGI: pid=" << _pid << " stdin fechou do outro lado";
		_stdinFd.close();
		return;
	}

	_inputOffset += static_cast<size_t>(n);
	if (_inputOffset >= _input.size()) {
		Logger::debug() << "CGI: pid=" << _pid << " body enviado, fechando stdin (EOF)";
		_stdinFd.close();
	}
}

void	CgiProcess::onStdoutReadable() {
	if (_stdoutFd.get() < 0)
		return;

	char	buf[8192];
	ssize_t	n = read(_stdoutFd.get(), buf, sizeof(buf));

	if (n <= 0) {
		// n == 0 é o EOF de verdade; n < 0 significa cano quebrado, e sem poder
		// consultar errno o tratamento é o mesmo: acabou.
		Logger::debug() << "CGI: pid=" << _pid << " EOF no stdout ("
			<< _output.size() << " bytes)";
		_stdoutFd.close();
		_phase = REAPING;
		return;
	}

	_output.append(buf, static_cast<size_t>(n));

	if (_output.size() > CGI_MAX_OUTPUT) {
		Logger::warning() << "CGI: pid=" << _pid << " passou de " << CGI_MAX_OUTPUT
			<< " bytes de saida, matando";
		killChild();
		_phase = FAILED;
	}
}

// =============================== fim de vida ===============================

/* Só o waitpid tira o filho morto da tabela de processos. Sem ele o processo
fica como zumbi (Z/defunct) pendurado no webserv - e é isso que a defesa
procura com o ps. WNOHANG porque perguntar não pode custar um bloqueio. */
void	CgiProcess::checkChild() {
	if (_pid <= 0 || _phase != REAPING)
		return;

	int		status = 0;
	pid_t	r = waitpid(_pid, &status, WNOHANG);

	if (r == 0)
		return; // ainda respirando, pergunta de novo na próxima volta
	if (r < 0) {
		_phase = COMPLETE; // já foi colhido; não dá pra insistir
		_pid = -1;
		return;
	}

	if (WIFEXITED(status))
		_exitStatus = WEXITSTATUS(status);
	else
		_exitStatus = -1; // morreu de sinal

	_pid = -1;
	_phase = COMPLETE;
	Logger::debug() << "CGI: colhido, exit=" << _exitStatus;
}

/* SIGKILL porque o filho pode estar ignorando qualquer coisa mais educada.
O waitpid aqui é bloqueante de propósito: depois de um SIGKILL o processo
morre na hora, então a espera é instantânea, e sair daqui sem colher deixaria
um zumbi. */
void	CgiProcess::killChild() {
	if (_pid > 0) {
		Logger::debug() << "CGI: matando pid=" << _pid;
		kill(_pid, SIGKILL);

		int	status = 0;
		waitpid(_pid, &status, 0);
		_pid = -1;
	}
	_stdinFd.close();
	_stdoutFd.close();
}
