#ifndef SOCKET_HPP
# define SOCKET_HPP

#include <string>
#include "FileDescriptor.hpp"

// Wrapper em cima de um FileDescriptor especializado em socket TCP: sabe
// bind/listen/accept/non-blocking. Não sabe nada de HTTP.
class Socket {
	private:
		FileDescriptor	_fd;

		Socket(const Socket& other);
		Socket& operator=(const Socket& other);

	public:
		Socket();                // cria um socket TCP novo (AF_INET, SOCK_STREAM) com SO_REUSEADDR
		explicit Socket(int fd); // embrulha um fd já existente (ex.: o retorno de accept())
		~Socket();

		int		getFd() const;
		void	close();

		void	bind(const std::string& host, unsigned short port);
		void	listen(int backlog);
		int		accept() const; // retorna o fd aceito; quem chama decide o que fazer com ele
		void	setNonBlocking();
};

#endif
