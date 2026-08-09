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
		Socket();
		explicit Socket(int fd);
		~Socket();

		int		getFd() const;
		void	close();

		void		bind(const std::string& host, unsigned short port);
		void		listen(int backlog);
		int			accept() const;
		void		setNonBlocking();
		static void	setCloexec(int fd); // aplica FD_CLOEXEC direto num fd cru para o CGI
		static void	setNonBlocking(int fd); // aplica O_NONBLOCK direto num fd cru, sem exigir posse (usado pelo fd vindo de accept())
};

#endif
