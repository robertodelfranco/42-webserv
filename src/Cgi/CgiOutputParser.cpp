#include "CgiOutputParser.hpp"
#include "../Request/HttpParser.hpp"
#include <sstream>
#include <vector>
#include <cstdlib>

static std::string::size_type	findHeadersEnd(const std::string& raw, size_t& skip) {
	std::string::size_type	crlf = raw.find("\r\n\r\n");
	std::string::size_type	lf = raw.find("\n\n");

	if (crlf != std::string::npos && (lf == std::string::npos || crlf < lf)) {
		skip = 4;
		return crlf;
	}

	if (lf != std::string::npos) {
		skip = 2;
		return lf;
	}

	return std::string::npos;
}

static std::string	trim(const std::string& s) {
	std::string::size_type	begin = s.find_first_not_of(" \t\r");
	std::string::size_type	end = s.find_last_not_of(" \t\r");

	if (begin == std::string::npos)
		return "";
	return s.substr(begin, end - begin + 1);
}

static std::string	buildStatusLine(const std::string& status) {
	std::string	value = trim(status);

	if (value.empty())
		return "200 OK";
	if (value.find(' ') != std::string::npos)
		return value;

	int	code = std::atoi(value.c_str());
	
	switch (code) {
		case 200:
			return "200 OK";
		case 201:
			return "201 Created";
		case 204:
			return "204 No Content";
		case 301:
			return "301 Moved Permanently";
		case 302:
			return "302 Found";
		case 400:
			return "400 Bad Request";
		case 403:
			return "403 Forbidden";
		case 404:
			return "404 Not Found";
		case 500:
			return "500 Internal Server Error";
		default:
			return value + " Status";
	}
}

bool	CgiOutputParser::toHttpResponse(const std::string& raw, bool keepAlive,
										std::string& out) {
	size_t					skip = 0;
	std::string::size_type	headersEnd = findHeadersEnd(raw, skip);

	if (headersEnd == std::string::npos)
		return false;

	const std::string	block = raw.substr(0, headersEnd);
	const std::string	body = raw.substr(headersEnd + skip);

	std::string					status;
	std::string					location;
	std::string					contentType;
	std::vector<std::string>	extras; // o que não é tratado aqui passa direto

	std::istringstream	iss(block);
	std::string			line;

	while (std::getline(iss, line)) {
		line = trim(line);
		if (line.empty())
			continue;

		std::string::size_type	colon = line.find(':');
		if (colon == std::string::npos)
			return false; // linha sem ':' dentro do bloco = saída inválida

		const std::string	name = toLower(trim(line.substr(0, colon)));
		const std::string	value = trim(line.substr(colon + 1));

		if (name == "status")
			status = value;
		else if (name == "content-type")
			contentType = value;
		else if (name == "content-length")
			continue;
		else if (name == "connection")
			continue;
		else {
			if (name == "location")
				location = value;
			extras.push_back(trim(line.substr(0, colon)) + ": " + value);
		}
	}

	if (status.empty() && !location.empty())
		status = "302";

	std::ostringstream	oss;
	oss << "HTTP/1.1 " << buildStatusLine(status) << "\r\n"
		<< "Content-Type: " << (contentType.empty() ? "text/html" : contentType) << "\r\n";

	for (size_t i = 0; i < extras.size(); ++i)
		oss << extras[i] << "\r\n";

	oss << "Content-Length: " << body.size() << "\r\n"
		<< "Connection: " << (keepAlive ? "keep-alive" : "close") << "\r\n"
		<< "\r\n";

	out = oss.str();
	out.append(body);
	return true;
}
