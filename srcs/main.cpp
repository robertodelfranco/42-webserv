#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>       // close()
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main()
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::cerr << "Erro ao criar socket\n";
        return EXIT_FAILURE;
    }
    
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Erro setsockopt\n";
        close(listen_fd);
        return EXIT_FAILURE;
    }
    
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;        // todas as interfaces
    addr.sin_port = htons(8080);               // porta 8080
    
    if (bind(listen_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Erro bind\n";
        close(listen_fd);
        return EXIT_FAILURE;
    }
    
    if (listen(listen_fd, 5) < 0) {
        std::cerr << "Erro listen\n";
        close(listen_fd);
        return EXIT_FAILURE;
    }
    
    std::cout << "Servidor escutando na porta 8080 …\n";
    
    while (true) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int conn_fd = accept(listen_fd,
                             reinterpret_cast<struct sockaddr*>(&cli_addr),
                             &cli_len);
        if (conn_fd < 0) {
            std::cerr << "Erro accept\n";
            continue;  // ou break dependendo do caso
        }
        
        char buffer[1024];
        std::memset(buffer, 0, sizeof(buffer));
        ssize_t n = read(conn_fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            std::cout << "Recebido: " << buffer << "\n";
            const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, world!";
            write(conn_fd, response, std::strlen(response));
        } else {
			std::cerr << "Erro read\n";
		}
        
        close(conn_fd);
    }
    
    close(listen_fd);
    return EXIT_SUCCESS;
}
