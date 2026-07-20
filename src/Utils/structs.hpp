#ifndef STRUCTS_HPP
# define STRUCTS_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sstream>
#include <unistd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdexcept>
#include "../Response/Response.hpp"
#include "../Response/ErrorResponse.hpp"
#include "../Request/HTTPRequest.hpp"

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
	std::string					path; // caminho padrão do location
	std::string					root; // caso algo sobreponha o destino root
	std::string					index; // for handle with get
	size_t 						client_max_body_size; //max body
	std::vector<std::string>	redir; // caso tenha redirect de paginas			
	bool						autoindex; // caso tenha ou não autoindex ligado
	size_t						allow_methods; // métodos permitidos "unificados" por bit (acesse por "&")
	long long					client_max_body__size; // caso tenha especificado dentro de location

	Location();
	Location(const std::string& path, const std::string& root, std::vector<std::string> redir, bool autoindex, size_t allow_methods, long long client_max_body__size);
};

#endif