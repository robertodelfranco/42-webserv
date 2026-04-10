#ifndef STRUCTS_HPP
# define STRUCTS_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <limits>
#include <map>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdexcept>

#define WHITEBOLD "\033[1;37m"
#define YELLOW "\033[1;33m"
#define GREEN "\033[1;32m"
#define CYAN "\033[1;36m"
#define RED "\033[1;31m"
#define RESET "\033[0m"

enum Methods
{
	GET = 1,
	POST = 2,
	DELETE = 4,
};

enum TokenType {
	UNKNOWN,
	DIRECTIVE,
	STRING,
	PATH,
	SYMBOL,
	EDGE_CASE,
	END_OF_STREAM
};

enum SocketState
{
	LISTENING,
	CONNECTED,
	CLOSED,
};

enum StatusCode 
{
	OK = 200,
	CREATED = 201,
	NO_CONTENT = 204,
	BAD_REQUEST = 400,
	NOT_FOUND = 404,
	METHOD_NOT_ALLOWED = 405,
	PAYLOAD_TOO_LARGE = 413,
	INTERNAL_SERVER_ERROR = 500,
};

enum DirectiveKind {
	SIMPLE,
	BLOCK
};

enum DirectiveContext {
	SERVER = 1, // 1 << 0
	LOCATION = 2 // 1 << 1
};

struct Token {
	TokenType	type;
	std::string	value;
	size_t		line;
	size_t		col;

	Token();
	Token(TokenType type, const std::string& value, size_t line, size_t col);
};

struct Listen {
	std::string		host; // 127.0.0.1 ou 0.0.0.0
	unsigned short	port; // porta

	Listen();
	Listen(const std::string& host, int port);
};

struct Location {
	std::string							path; // caminho padrão do location
	std::string							root; // caso algo sobreponha o destino root
	std::string							cgi_type; // caso tenha cgi
	std::string							cgi_path; // caminho para o executavel do cgi (pyhton3, Go e etc)
	std::vector<std::string>			index_files; // index definido no escopo do location
	std::map<std::string, std::string>	redir; // caso tenha redirect de paginas			
	bool								autoindex; // caso tenha ou não autoindex ligado
	size_t								allow_methods; // métodos permitidos "unificados" por bit (acesse por "&")
	long long							client_max_body_size; // caso tenha especificado dentro de location

	Location();
};

#endif