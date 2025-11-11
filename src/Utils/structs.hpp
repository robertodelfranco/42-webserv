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
	Token(TokenType type, const std::string& value, int line, int col);
};

struct Listen {
	std::string		host; // 127.0.0.1 ou 0.0.0.0
	unsigned short	port; // porta

	Listen();
	Listen(const std::string& host, int port);
};

struct Location {
	std::string					path; // caminho padrão do location
	std::string					root; // caso algo sobreponha o destino root
	std::vector<std::string>	allow_methods; // GET, POST e DELETE
	bool						autoindex; // caso tenha ou não autoindex ligado

	Location();
	Location(const std::string& path, const std::string& root, const std::vector<std::string>& allow_methods, bool autoindex);
};

#endif