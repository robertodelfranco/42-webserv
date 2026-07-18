#ifndef SOCKET_HPP
# define SOCKET_HPP

#include <string>

// Wrapper RAII em cima de UM file descriptor de socket. Não sabe nada de
// HTTP — só bind/listen/accept/non-blocking. Dono único do fd: o destrutor
// fecha, por isso não é copiável (evita dois Socket fechando o mesmo fd).
class Socket {
	private:
		int	_fd;

		Socket(const Socket& other);
		Socket& operator=(const Socket& other);

	public:
		Socket();                // socket(AF_INET, SOCK_STREAM, 0) + SO_REUSEADDR
		explicit Socket(int fd); // embrulha um fd já existente (ex.: retorno de accept())
		~Socket();               // fecha o fd, se ainda estiver aberto

		int		getFd() const;
		void	close();

		void	bind(const std::string& host, unsigned short port);
		void	listen(int backlog);
		int		accept() const; // retorna o fd aceito; quem chama decide o que fazer com ele
		void	setNonBlocking();
};

#endif
