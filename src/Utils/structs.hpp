#ifndef STRUCTS_HPP
#define STRUCTS_HPP

#include <iostream>
#include <vector>
#include <map>

enum Method
{
	GET,
	POST,
	DELETE,
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

#endif