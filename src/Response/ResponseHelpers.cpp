#include "ResponseHelpers.hpp"

#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

#include "../Request/HttpRequest.hpp"
#include "../ServerConfig/Location.hpp"

namespace ResponseHelpers {
	std::string joinPath(const std::string &root, const std::string &path) {
		if (root.empty())
			return path;
		if (path.empty())
			return root;
		if (root[root.size() - 1] == '/' && path[0] == '/')
			return root + path.substr(1);
		if (root[root.size() - 1] != '/' && path[0] != '/')
			return root + "/" + path;
		return root + path;
	}

	bool readFileToString(const std::string &path, std::string &content) {
		std::ifstream ifs(path.c_str(), std::ios::in | std::ios::binary);
		if (!ifs)
			return false;
		std::ostringstream ss;
		ss << ifs.rdbuf();
		content = ss.str();
		return true;
	}

	std::string buildAutoindexHtml(const std::string &dir_path, const std::string &request_path) {
		std::ostringstream html;
		html << "<html><head><title>Index of " << request_path << "</title></head>";
		html << "<body><h1>Index of " << request_path << "</h1><ul>";

		DIR *dir = opendir(dir_path.c_str());
		if (!dir)
			return std::string();

		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			if (entry->d_name[0] == '.')
				continue;

			std::string href = entry->d_name;
			std::string full_path = dir_path;
			if (!full_path.empty() && full_path[full_path.size() - 1] != '/')
				full_path += '/';
			full_path += entry->d_name;

			struct stat entry_stat;
			if (stat(full_path.c_str(), &entry_stat) == 0 && S_ISDIR(entry_stat.st_mode))
				href += '/';
			html << "<li><a href=\"" << href << "\">" << entry->d_name << "</a></li>";
		}
		closedir(dir);

		html << "</ul></body></html>";
		return html.str();
	}

	bool resolveTargetPath(const HttpRequest &request, const Location &location, std::string &target_path, bool &autoindex) {
		autoindex = false;
		target_path = joinPath(location.getRoot(), request.getPath());

		struct stat file_stat;
		if (stat(target_path.c_str(), &file_stat) == -1)
			return false;

		if (S_ISDIR(file_stat.st_mode)) {
			std::string index_path = target_path;
			if (!index_path.empty() && index_path[index_path.size() - 1] != '/')
				index_path += '/';
			if (location.getIndexFiles().empty())
				return false;
			index_path += location.getIndexFiles().front();

			if (stat(index_path.c_str(), &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
				target_path = index_path;
			} else if (location.getAutoIndex()) {
				autoindex = true;
			} else {
				return false;
			}
		}

		return true;
	}

	std::string mimeTypeForPath(const std::string &path) {
		size_t dot = path.find_last_of('.');
		if (dot == std::string::npos)
			return "application/octet-stream";

		std::string ext = path.substr(dot);
		if (ext == ".html") return "text/html";
		if (ext == ".css") return "text/css";
		if (ext == ".js") return "application/javascript";
		return "application/octet-stream";
	}

	std::string extractFileName(const std::string &path) {
		std::string::size_type slash = path.find_last_of('/');
		if (slash == std::string::npos || slash + 1 >= path.size())
			return std::string();
		return path.substr(slash + 1);
	}
}