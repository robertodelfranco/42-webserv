#include "../Response/ResponseHandle.hpp"
#include "../Utils/structs.hpp"

ResponseBuilder::ResponseBuilder(const HTTPRequest &req, const Location &loc)
	: request(req), location(loc) {}

bool ResponseBuilder::isMethodAllowed(const std::string &method) const { 
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
	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos) return "application/octet-stream";
    
	std::string ext = path.substr(dot); //find last dot and return the correct path
	if (ext == ".html") return "text/html";
	if (ext == ".css") return "text/css";
	if (ext == ".js") return "application/javascript";
	return "application/octet-stream";
}

Response ResponseBuilder::handleGET(){
	Response resp;
	std::string file_path;

	file_path = location.root + request.getPath();

	struct stat file_stat;
	if (stat(file_path.c_str(), &file_stat) == -1){
		return ErrorResponse(NOT_FOUND);
	}

	if (S_ISDIR(file_stat.st_mode)){
		
		std::string index_path;
		index_path = file_path;
		if (file_path[file_path.size() - 1] != '/')
			index_path += '/';
		index_path += location.index;

		if (stat(index_path.c_str(), &file_stat) == 0 && S_ISREG(file_stat.st_mode)){
			file_path = index_path;
		} else if (location.autoindex){
			return generateAutoindex(file_path); //need implementation
		} else {
			return ErrorResponse(NOT_FOUND);
		}
	}

	if (access(file_path.c_str(), R_OK) != 0) {
		resp.setStatus(OK);
		resp.setHeader("Content-Type", getMimeType(file_path)); //need implementation
	} else {
		return ErrorResponse(INTERNAL_SERVER_ERROR);
	}
	return resp;
}

Response ResponseBuilder::handleHEAD() {
    Response resp = handleGET();
    resp.setBody("");
    return resp;
}

Response ResponseBuilder::generateAutoindex(const std::string &dir_path){
	Response resp;
	DIR *dir = opendir(dir_path.c_str());
	if (!dir) return ErrorResponse(INTERNAL_SERVER_ERROR);

	std::ostringstream html;
	html << "<html><head><title>Index of " << request.getPath() << "</title></head>";
    html << "<body><h1>Index of " << request.getPath() << "</h1><ul>";
	
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') continue;

		std::string href = entry->d_name;
		struct stat entry_stat;
		std::string full_path = dir_path;
		if (full_path[full_path.size() - 1] != '/')
			full_path += '/';
		full_path += entry->d_name;

		if (stat(full_path.c_str(), &entry_stat) == 0 && S_ISDIR(entry_stat.st_mode))
			href += '/';
		html << "<li><a href=\"" << href << "\">" << entry->d_name << "</a></li>";
	}

	html << "</ul></body></html>";
	closedir(dir);

	resp.setStatus(OK);
	resp.setBody(html.str());
	resp.setHeader("Content-Type", "text/html");
	return resp;
}

Response ResponseBuilder::handlePOST(){
	Response resp;
	size_t body_size;
	body_size = request.getBody().size();
	if (body_size > location.client_max_body_size){
		return ErrorResponse(PAYLOAD_TOO_LARGE);
	}

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

	if (method == "GET") return handleGET();
    if (method == "POST") return handlePOST();
    if (method == "DELETE") return handleDELETE();
    
    return handleMethodAllowed();
}

