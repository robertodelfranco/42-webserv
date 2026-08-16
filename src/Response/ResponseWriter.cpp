#include "ResponseWriter.hpp"

#include <sstream>

#include "HttpResponse.hpp"

std::string ResponseWriter::write(const HttpResponse &response)
{
	std::ostringstream out;
	out << "HTTP/1.1 " << response.getStatus() << " " << response.getStatusMessage() << "\r\n";

	const std::map<std::string, std::string> &headers = response.getHeaders();
	if (headers.find("Content-Length") == headers.end()) {
		std::ostringstream oss;
		oss << response.getBody().size();
		out << "Content-Length: " << oss.str() << "\r\n";
	}

	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
		out << it->first << ": " << it->second << "\r\n";

	out << "\r\n";
	out << response.getBody();
	return out.str();
}