#ifndef STRUCTS_HPP
# define STRUCTS_HPP

#include <iostream>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define YELLOW "\033[1;33m"
#define GREEN "\033[1;32m"
#define CYAN "\033[1;36m"
#define RED "\033[1;31m"
#define RESET "\033[0m"

enum Method
{
	GET,
	POST,
	DELETE,
};

enum TokenType {
	UNKNOWN,
	IDENTIFIER,
	NUMBER,
	STRING,
	PATH,
	LBRACE,
	RBRACE,
	SEMICOLON,
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

struct Token {
	TokenType	type;
	std::string	value;
	int			line;
	int			col;

	Token();
};

struct Listen {
	std::string		host; // 127.0.0.1 ou 0.0.0.0
	unsigned short	port; // porta

	Listen();
};

struct Location {
	bool						autoindex; // caso tenha ou não autoindex ligado
	std::string					path; // caminho padrão do location
	std::string					root_override; // caso algo sobreponha o destino root
	std::vector<std::string>	allow_methods; // GET, POST e DELETE

	Location();
};

#endif