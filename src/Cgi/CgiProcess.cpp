#include "CgiProcess.hpp"
#include "../Network/Socket.hpp"
#include "../Utils/Logger.hpp"
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

// mesmo raciocinio do IO_BUFFER_SIZE da Connection: 8192 gerava syscall
// demais lendo o stdout de um CGI grande (echo de 100MB no cgi_tester)
static const size_t	CGI_READ_BUFFER_SIZE = 65536;

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

CgiProcess::~CgiProcess() {
	if (_pid > 0)
		killChild();
}

void	CgiProcess::runChild(int inPipe[2], int outPipe[2]) {
	if (dup2(inPipe[0], STDIN_FILENO) < 0 || dup2(outPipe[1], STDOUT_FILENO) < 0)
		_exit(127);

	// as 4 pontas originais já estão duplicadas em 0 e 1, não servem mais
	if (inPipe[0] > STDERR_FILENO)
		::close(inPipe[0]);
	if (inPipe[1] > STDERR_FILENO)
		::close(inPipe[1]);
	if (outPipe[0] > STDERR_FILENO)
		::close(outPipe[0]);
	if (outPipe[1] > STDERR_FILENO)
		::close(outPipe[1]);

	// o script espera rodar de dentro da própria pasta
	if (!_workDir.empty() && chdir(_workDir.c_str()) < 0)
		_exit(127);

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
		runChild(inPipe, outPipe);

	::close(inPipe[0]);
	::close(outPipe[1]);

	_stdinFd.reset(inPipe[1]);
	_stdoutFd.reset(outPipe[0]);

	try {
		// FD_CLOEXC pra que o PRÓXIMO CGI não herde esses fds
		Socket::setNonBlocking(_stdinFd.get());
		Socket::setNonBlocking(_stdoutFd.get());
	} catch (const std::exception& e) {
		Logger::error() << "CGI: " << e.what();
		killChild();
		_phase = FAILED;
		return false;
	}

	// GET ou algo sem body, o EOF do stdin é imediato
	if (_input.empty())
		_stdinFd.close();

	_startTime = std::time(NULL);
	_phase = RUNNING;
	Logger::debug() << "CGI: pid=" << _pid << " stdin=" << _stdinFd.get()
		<< " stdout=" << _stdoutFd.get() << " body=" << _input.size() << " bytes";

	return true;
}

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

void	CgiProcess::closeStdin() {
	_stdinFd.close();
}

void	CgiProcess::onStdinWritable() {
	if (_stdinFd.get() < 0 || _inputOffset >= _input.size())
		return;

	ssize_t	n = write(_stdinFd.get(), _input.data() + _inputOffset, _input.size() - _inputOffset);

	if (n <= 0) {
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

	char	buf[CGI_READ_BUFFER_SIZE];
	ssize_t	n = read(_stdoutFd.get(), buf, sizeof(buf));

	if (n <= 0) {
		// n == 0 é o EOF de verdade e n < 0 significa cano quebrado
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

void	CgiProcess::checkChild() {
	if (_pid <= 0 || _phase != REAPING)
		return;

	int		status = 0;
	pid_t	r = waitpid(_pid, &status, WNOHANG);

	if (r == 0)
		return; // ainda respirando, pergunta de novo na próxima volta
	if (r < 0) {
		_phase = COMPLETE; // já foi colhido
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
