#ifndef CGIPROCESS_HPP
# define CGIPROCESS_HPP

#include <string>
#include <vector>
#include <ctime>
#include <sys/types.h>
#include "../Network/FileDescriptor.hpp"

// Sem isso um CGI maluco (while True: print) enche a RAM do servidor
#define CGI_MAX_OUTPUT (300 * 1024 * 1024) // reveja isso depois

// timeout próprio do cgi
#define CGI_TIMEOUT 600

class CgiProcess {
	public:
		enum Phase {
			RUNNING,	// filho vivo
			REAPING,	// EOF no stdout
			COMPLETE,	// colhido, _output final e _exitStatus registrados
			TIMED_OUT,	// estourou o CGI_TIMEOUT, mata o filho
			FAILED		// pipe/fork falhou ou o output passou do teto máximo
		};

		CgiProcess(const std::string& interpreter, const std::string& scriptPath,
				   const std::string& workDir, const std::vector<std::string>& env,
				   const std::string& body);
		~CgiProcess();

		// false = não deu pra forkar
		bool	start();

		// O EventLoop precisa disso pra decidir quais pipes entram no poll
		bool	wantsWriteInput() const;
		bool	wantsReadOutput() const;
		int		getStdinFd() const;
		int		getStdoutFd() const;

		void	onStdinWritable();
		void	onStdoutReadable();
		void	closeStdin();	// POLLERR/POLLNVAL no cano de entrada

		void	checkChild();	// REAPING -> waitpid(WNOHANG) -> COMPLETE
		void	killChild();	// SIGKILL + waitpid

		Phase				getPhase() const;
		int					getExitStatus() const;
		const std::string&	getOutput() const;
		std::time_t			getStartTime() const;

	private:
		std::string					_interpreter;	// /usr/bin/python3
		std::string					_scriptPath;	// caminho ABSOLUTO do script
		std::string					_workDir;		// diretório do script (chdir)
		std::vector<std::string>	_env;			// dono das strings do envp
		pid_t						_pid;
		FileDescriptor				_stdinFd;		// ponta de ESCRITA do cano de entrada
		FileDescriptor				_stdoutFd;		// ponta de LEITURA do cano de saída
		std::string					_input;			// body já decodificado
		size_t						_inputOffset;
		std::string					_output;		// acumula até o EOF
		std::time_t					_startTime;
		Phase						_phase;
		int							_exitStatus;

		CgiProcess(const CgiProcess& other);
		CgiProcess& operator=(const CgiProcess& other);

		void	runChild(int inPipe[2], int outPipe[2]);
};

#endif
