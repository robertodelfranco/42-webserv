#ifndef CGIPROCESS_HPP
# define CGIPROCESS_HPP

#include <string>
#include <vector>
#include <ctime>
#include <sys/types.h>
#include "../Network/FileDescriptor.hpp"

/* Teto de saída do script: acima disso o filho é morto e a request vira 502.
Sem isso um CGI maluco (while True: print) enche a RAM do servidor. */
#define CGI_MAX_OUTPUT (10 * 1024 * 1024)

/* Prazo do CGI, contado do fork. É PROPOSITALMENTE diferente do
CONNECTION_TIMEOUT: uma conexão em CGI não está ociosa, está esperando. */
#define CGI_TIMEOUT 10

/* O processo filho e os dois canos que ligam ele ao servidor.
Vive pendurado no Connection (o CgiHandler que o criou morre logo depois),
e o destructor é a rede de segurança: qualquer caminho de morte da conexão
mata o filho e fecha os pipes de graça, via RAII.

Nunca faz I/O por conta própria: só lê ou escreve quando o poll() avisou,
e os dois pipes ficam armados AO MESMO TEMPO. Tratar "mandar o body" e
"ler a resposta" como fases sequenciais é o deadlock clássico do pipe de
64KB: o script trava no print (cano de saída cheio) enquanto o servidor
trava no write (cano de entrada cheio), cada um esperando o outro. */
class CgiProcess {
	public:
		enum Phase {
			RUNNING,	// filho vivo. O sub-estado é ORTOGONAL, dado pelos fds:
						//   _stdinFd  aberto <=> ainda há body pra enviar
						//   _stdoutFd aberto <=> o EOF ainda não veio
			REAPING,	// EOF no stdout: falta só o waitpid confirmar a morte
			COMPLETE,	// colhido, _output final e _exitStatus registrados
			TIMED_OUT,	// estourou o CGI_TIMEOUT, filho morto na marra -> 504
			FAILED		// pipe/fork falhou ou o output passou do teto -> 500/502
		};

		CgiProcess(const std::string& interpreter, const std::string& scriptPath,
				   const std::string& workDir, const std::vector<std::string>& env,
				   const std::string& body);
		~CgiProcess();

		// Faz o ritual pipe/fork/dup2/execve. false = não deu nem pra forkar.
		bool	start();

		// O EventLoop pergunta isso pra decidir quais pipes entram no poll().
		bool	wantsWriteInput() const;
		bool	wantsReadOutput() const;
		int		getStdinFd() const;
		int		getStdoutFd() const;

		// Um write/read por evento de poll, nunca em loop até EAGAIN (nada de
		// olhar errno depois de read/write, o subject proíbe).
		void	onStdinWritable();
		void	onStdoutReadable();
		void	closeStdin();	// POLLERR/POLLNVAL no cano de entrada

		void	checkChild();	// REAPING -> waitpid(WNOHANG) -> COMPLETE
		void	killChild();	// SIGKILL + waitpid, pro chef que travou

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

		/* fd é posse, não valor: copiar um CgiProcess daria dois donos do mesmo
		pipe e dois waitpid no mesmo pid. */
		CgiProcess(const CgiProcess& other);
		CgiProcess& operator=(const CgiProcess& other);

		void	runChild(int inPipe[2], int outPipe[2]);
};

#endif
