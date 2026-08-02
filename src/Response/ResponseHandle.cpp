#include "../Response/ResponseHandle.hpp"
#include "../Utils/structs.hpp"

namespace {
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

	bool resolveTargetPath(const HTTPRequest &request, const Location &location, std::string &target_path, bool &autoindex) {
		autoindex = false;
		target_path = joinPath(location.root, request.getPath());

		struct stat file_stat;
		if (stat(target_path.c_str(), &file_stat) == -1)
			return false;

		if (S_ISDIR(file_stat.st_mode)) {
			std::string index_path = target_path;
			if (!index_path.empty() && index_path[index_path.size() - 1] != '/')
				index_path += '/';
			if (location.index_files.empty())
				return false;
			index_path += location.index_files.front();

			if (stat(index_path.c_str(), &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
				target_path = index_path;
			} else if (location.autoindex) {
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

ResponseBuilder::ResponseBuilder(const HTTPRequest &req, const Location &loc)
	: request(req), location(loc) {}

bool ResponseBuilder::isMethodAllowed(const std::string &method) const { 
	if (method == "HEAD")
		return (location.allow_methods & GET) != 0;
	Methods m = methodToEnum(method);
	if (m == 0) return false; //don't find get post or delete
	return (location.allow_methods & m) != 0; // return one of the 3 methods
}

Methods ResponseBuilder::methodToEnum(const std::string &method) const {
	if (method == "GET") return GET;
	if (method == "POST") return POST;
    if (method == "DELETE") return DELETE;
    return static_cast<Methods>(0);
}

//this function is resposable for find the type of the file
std::string ResponseBuilder::getMimeType(const std::string &path) const {
	return mimeTypeForPath(path);
}

Response ResponseBuilder::handleGET(){
	Response resp;
	std::string file_path;
	bool autoindex = false;

	if (!resolveTargetPath(request, location, file_path, autoindex))
		return ErrorResponse(NOT_FOUND);

	if (autoindex) {
		std::string html = buildAutoindexHtml(file_path, request.getPath());
		if (html.empty())
			return ErrorResponse(INTERNAL_SERVER_ERROR);

		resp.setStatus(OK);
		resp.setHeader("Content-Type", "text/html");
		resp.setBody(html);
		return resp;
	}

	struct stat file_stat;
	if (stat(file_path.c_str(), &file_stat) == -1){
		return ErrorResponse(NOT_FOUND);
	}

	if (access(file_path.c_str(), R_OK) != 0) {
		return ErrorResponse(FORBIDDEN);
	}

	if (!resp.setBodyFromFile(file_path))
		return ErrorResponse(INTERNAL_SERVER_ERROR);

	resp.setStatus(OK);
	resp.setHeader("Content-Type", mimeTypeForPath(file_path));
	return resp;
}

Response ResponseBuilder::handleHEAD() {
	Response resp;
	std::string file_path;
	bool autoindex = false;

	if (!resolveTargetPath(request, location, file_path, autoindex))
		return ErrorResponse(NOT_FOUND);

	if (autoindex) {
		std::string html = buildAutoindexHtml(file_path, request.getPath());
		if (html.empty())
			return ErrorResponse(INTERNAL_SERVER_ERROR);

		std::ostringstream oss;
		oss << html.size();
		resp.setStatus(OK);
		resp.setHeader("Content-Type", "text/html");
		resp.setHeader("Content-Length", oss.str());
		return resp;
	}

	struct stat file_stat;
	if (stat(file_path.c_str(), &file_stat) == -1)
		return ErrorResponse(NOT_FOUND);
	if (access(file_path.c_str(), R_OK) != 0)
		return ErrorResponse(FORBIDDEN);

	std::ostringstream oss;
	oss << file_stat.st_size;
	resp.setStatus(OK);
	resp.setHeader("Content-Type", mimeTypeForPath(file_path));
	resp.setHeader("Content-Length", oss.str());
	return resp;
}

Response ResponseBuilder::generateAutoindex(const std::string &dir_path){
	Response resp;
	std::string html = buildAutoindexHtml(dir_path, request.getPath());
	if (html.empty())
		return ErrorResponse(INTERNAL_SERVER_ERROR);

	resp.setStatus(OK);
	resp.setBody(html);
	resp.setHeader("Content-Type", "text/html");
	return resp;
}

Response ResponseBuilder::handlePOST(){
	Response resp;
	size_t body_size = request.getBody().size();
	if (body_size > location.client_max_body_size){
		return ErrorResponse(PAYLOAD_TOO_LARGE);
	}

	if (location.upload_path.empty())
		return ErrorResponse(INTERNAL_SERVER_ERROR);

	std::string file_name = extractFileName(request.getPath());
	if (file_name.empty())
		return ErrorResponse(BAD_REQUEST);

	std::string target_path = joinPath(location.upload_path, file_name);
	std::ofstream ofs(target_path.c_str(), std::ios::binary | std::ios::trunc);
	if (!ofs)
		return ErrorResponse(INTERNAL_SERVER_ERROR);

	ofs.write(request.getBody().data(), request.getBody().size());
	if (!ofs)
		return ErrorResponse(INTERNAL_SERVER_ERROR);

	resp.setStatus(CREATED);
	resp.setHeader("Content-Type", "text/plain");
	resp.setHeader("Location", request.getPath());
	resp.setBody("");

	return resp;
}

Response ResponseBuilder::handleDELETE() {
	Response resp;
	std::string file_path;
	file_path = location.root + request.getPath();

	struct stat file_stat;
	if (stat(file_path.c_str(), &file_stat) == -1){
		return ErrorResponse(NOT_FOUND);
	}

	if (S_ISDIR(file_stat.st_mode)) {
		return ErrorResponse(METHOD_NOT_ALLOWED);
	}

	if (unlink(file_path.c_str()) == 0) {
		resp.setStatus(NO_CONTENT);
	} else {
		return ErrorResponse(INTERNAL_SERVER_ERROR);
	}

	return resp;
}

Response ResponseBuilder::handleMethodAllowed() {
	return ErrorResponse(METHOD_NOT_ALLOWED);
}

Response ResponseBuilder::build() {
	std::string method = request.getMethod();

	if (!isMethodAllowed(method))
		return handleMethodAllowed();

	if (method == "HEAD") return handleHEAD();
	if (method == "GET") return handleGET();
    if (method == "POST") return handlePOST();
    if (method == "DELETE") return handleDELETE();
    
    return handleMethodAllowed();
}

