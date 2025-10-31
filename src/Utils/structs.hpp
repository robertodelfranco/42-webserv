#ifndef STRUCTS_HPP
#define STRUCTS_HPP

#include <iostream>
#include <vector>
#include <map>

enum method
{
	GET,
	POST,
	DELETE,
} 	method;

enum socket_state
{
	LISTENING,
	CONNECTED,
	CLOSED,
}	socket_state;

enum status_code 
{
	OK = 200,
	CREATED = 201,
	NO_CONTENT = 204,
	BAD_REQUEST = 400,
	NOT_FOUND = 404,
	METHOD_NOT_ALLOWED = 405,
	PAYLOAD_TOO_LARGE = 413,
	INTERNAL_SERVER_ERROR = 500,
}	status_code;

#endif