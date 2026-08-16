#ifndef RESPONSE_HELPERS_HPP
#define RESPONSE_HELPERS_HPP

#include <string>

class HttpRequest;
class Location;

namespace ResponseHelpers {
	std::string joinPath(const std::string &root, const std::string &path);
	bool readFileToString(const std::string &path, std::string &content);
	std::string buildAutoindexHtml(const std::string &dir_path, const std::string &request_path);
	bool resolveTargetPath(const HttpRequest &request, const Location &location, std::string &target_path, bool &autoindex);
	std::string mimeTypeForPath(const std::string &path);
	std::string extractFileName(const std::string &path);
}

#endif /* RESPONSE_HELPERS_HPP */