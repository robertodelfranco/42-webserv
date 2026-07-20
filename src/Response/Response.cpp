#include "Response.hpp"

Response::Response()
: status_code(200), status_message("OK"), body(), close_after_send(true){}

Response::Response(const Response &other)
: status_code(other.status_code), status_message(other.status_message), body(other.body), headers(other.headers), close_after_send(other.close_after_send) {}

Response &Response::operator=(const Response &other){
	if (this != &other){
		status_code = other.status_code;
		status_message = other.status_message;
		body = other.body;
		headers = other.headers;
		close_after_send = other.close_after_send;
	}
	return *this;
}

Response::~Response() {}

//just to know wich code return
std::string Response::statusMessageCode(int code) const {
	switch(code){
		case OK: return "OK";
		case CREATED: return "Created";
		case NO_CONTENT: return "No Content";
		case BAD_REQUEST: return "Bad Request";
		case NOT_FOUND: return "Not Found";
		case METHOD_NOT_ALLOWED: return "Method Not Allowed";
		case PAYLOAD_TOO_LARGE: return "Payload too Large";
		case INTERNAL_SERVER_ERROR: return "Internal Server Error";
		default: return "Unknown";
	}
}

// just set de code and message
void Response::setStatus(int code){ 
	status_code = code;
	status_message = statusMessageCode(code);
}
// add or remove headers on headers map
void Response::setHeader(const std::string &key, const std::string &value){
	headers[key] = value;
}

// set body and update Content-Length
void Response::setBody(const std::string &b){
	body = b;
	std::ostringstream oss;
	oss << body.size();
	headers["Content-Length"] = oss.str();
}

// try read the file of the path and make the body
bool Response::setBodyFromFile(const std::string &path){
	std::ifstream ifs(path.c_str(), std::ios::in | std::ios::binary); //open the file for read in binary mode
	if (!ifs) return false;
	std::ostringstream ss; //write the string on buffer ss, string stream
	ss << ifs.rdbuf(); //copy the buff to ostringstream
	setBody(ss.str()); //make the body with the buffer
	return true;
}

//just a flag for close
void Response::setCloseAfterSend(bool close){
	close_after_send = close;
	if (close)
		headers["Connection"] = "close";
	else
		headers["Connection"] = "keep-alive";
}

// glue everthing to make the message error.
std::string Response::toString() const{
	std::ostringstream out;
	out << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";
	if (headers.find("Content-Length") == headers.end()){
		std::ostringstream oss;
		oss << body.size();
		out << "Content-Length: " << oss.str() << "\r\n";
	}

	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it){
		out << it->first << ": " << it->second << "\r\n";
	}
	out << "\r\n";
	out << body;
	return out.str();
}

int Response::getStatus() const {
	return status_code;
}

const std::map<std::string, std::string>& Response::getHeaders() const {
	return headers;
}



