#include "HttpResponse.hpp"

#include <sstream>

HttpResponse::HttpResponse()
: status_code_(200), status_message_("OK"), body_(), headers_(), close_after_send_(true)
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
		case BAD_REQUEST: return "Bad Request";
		case FORBIDDEN: return "Forbidden";
		case NOT_FOUND: return "Not Found";
		case METHOD_NOT_ALLOWED: return "Method Not Allowed";
		case PAYLOAD_TOO_LARGE: return "Payload too Large";
		case INTERNAL_SERVER_ERROR: return "Internal Server Error";
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
