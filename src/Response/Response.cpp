#include "Response.hpp"

#include "ResponseWriter.hpp"

Response::Response()
: HttpResponse()
{
}

Response::Response(const Response &other)
: HttpResponse(other)
{
}

Response &Response::operator=(const Response &other)
{
	if (this != &other)
		HttpResponse::operator=(other);
	return *this;
}

Response::~Response() {}

std::string Response::toString() const
{
	return ResponseWriter::write(*this);
}



