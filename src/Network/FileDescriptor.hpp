#ifndef FILEDESCRIPTOR_HPP
# define FILEDESCRIPTOR_HPP

// Dono único de UM file descriptor. Não sabe se é socket, pipe ou
// arquivo, só garante que close() acontece exatamente uma vez. Reusado
// por Socket (fd de rede) e, mais pra frente, pelos pipes do CGI.
class FileDescriptor {
	private:
		int	_fd;

		/* muito importante isso aqui! copiar fd num wrapper desse vai dar
		problema. quando é necessário, fd se copia com dup(), dup2(). (edu) */
		FileDescriptor(const FileDescriptor& other);
		FileDescriptor& operator=(const FileDescriptor& other);

	public:
		explicit FileDescriptor(int fd = -1);
		~FileDescriptor();

		int		get() const;
		void	close();
		int		release();
};

#endif
