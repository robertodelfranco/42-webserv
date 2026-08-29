#include "HttpResponse.hpp"

#include <sstream>

/* close_after_send precisa ser false, quem decide keep-alive é a Connection
a resposta so opina quando o proprio conteudo obriga a fechar, como nos erros */
HttpResponse::HttpResponse()
: status_code_(200), status_message_("OK"), body_(), headers_(), close_after_send_(false)
{
}

HttpResponse::HttpResponse(const HttpResponse &other)
{
	*this = other;
}

HttpResponse &HttpResponse::operator=(const HttpResponse &other)
{
	if (this != &other) {
		status_code_ = other.status_code_;
		status_message_ = other.status_message_;
		body_ = other.body_;
		headers_ = other.headers_;
		close_after_send_ = other.close_after_send_;
	}
	return *this;
}

HttpResponse::~HttpResponse()
{
}

std::string HttpResponse::statusMessageCode(int code) const
{
	switch (code) {
		case OK: return "OK";
		case CREATED: return "Created";
		case NO_CONTENT: return "No Content";
		case MOVED_PERMANENTLY: return "Moved Permanently";
		case FOUND: return "Found";
		case SEE_OTHER: return "See Other";
		case TEMPORARY_REDIRECT: return "Temporary Redirect";
		case PERMANENT_REDIRECT: return "Permanent Redirect";
		case BAD_REQUEST: return "Bad Request";
		case FORBIDDEN: return "Forbidden";
		case NOT_FOUND: return "Not Found";
		case METHOD_NOT_ALLOWED: return "Method Not Allowed";
		case REQUEST_TIMEOUT: return "Request Timeout";
		case PAYLOAD_TOO_LARGE: return "Content Too Large";
		case HEADERS_TOO_LARGE: return "Request Header Fields Too Large";
		case INTERNAL_SERVER_ERROR: return "Internal Server Error";
		case NOT_IMPLEMENTED: return "Not Implemented";
		case BAD_GATEWAY: return "Bad Gateway";
		case GATEWAY_TIMEOUT: return "Gateway Timeout";
		case HTTP_VERSION_NOT_SUPPORTED: return "HTTP Version Not Supported";
		default: return "Unknown";
	}
}

void HttpResponse::setStatus(int code)
{
	status_code_ = code;
	status_message_ = statusMessageCode(code);
}

void HttpResponse::setHeader(const std::string &key, const std::string &value)
{
	headers_[key] = value;
}

void HttpResponse::setBody(const std::string &body)
{
	body_ = body;
	std::ostringstream oss;
	oss << body_.size();
	headers_["Content-Length"] = oss.str();
}

void HttpResponse::setCloseAfterSend(bool close)
{
	close_after_send_ = close;
	if (close)
		headers_["Connection"] = "close";
	else
		headers_["Connection"] = "keep-alive";
}

int HttpResponse::getStatus() const
{
	return status_code_;
}

const std::string &HttpResponse::getStatusMessage() const
{
	return status_message_;
}

const std::string &HttpResponse::getBody() const
{
	return body_;
}

bool HttpResponse::getCloseAfterSend() const
{
	return close_after_send_;
}

const std::map<std::string, std::string> &HttpResponse::getHeaders() const
{
	return headers_;
}
