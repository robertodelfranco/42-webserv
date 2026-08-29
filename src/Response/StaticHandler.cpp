#include "StaticHandler.hpp"
#include "../Network/Connection.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>


StaticHandler::StaticHandler(const HttpRequest &req, const Location &loc)
	: request(req), location(loc) {}

bool StaticHandler::isMethodAllowed(const std::string &method) const {
	size_t bit = methodToBit(method); // tabela única, mora na Location.hpp agora

	if (bit == 0)
		return false;
	return (location.getAllowMethods() & bit) != 0;
}

/* A listagem do autoindex gera href relativo ("arquivo.txt"). Sem a barra
final na URI o browser resolve contra o pai ("/pasta" + "arquivo.txt" =
"/arquivo.txt") e todo link quebra. O nginx resolve com 301 pra versão
com barra, e é o que fazemos */
Response StaticHandler::redirectToDirSlash() const {
	Response resp;

	resp.setStatus(MOVED_PERMANENTLY);
	resp.setHeader("Location", request.getPath() + "/");
	resp.setBody("");
	return resp;
}

bool StaticHandler::needsDirSlash(bool autoindex) const {
	const std::string&	path = request.getPath();

	return autoindex && !path.empty() && path[path.size() - 1] != '/';
}

Response StaticHandler::handleGET(){
	Response resp;
	std::string file_path;
	bool autoindex = false;

	if (!ResponseHelpers::resolveTargetPath(request, location, file_path, autoindex))
		return ErrorResponse(NOT_FOUND);

	if (needsDirSlash(autoindex))
		return redirectToDirSlash();

	if (autoindex) {
		std::string html = ResponseHelpers::buildAutoindexHtml(file_path, request.getPath());
		if (html.empty())
			return ErrorResponse(INTERNAL_SERVER_ERROR);

		resp.setStatus(OK);
		resp.setHeader("Content-Type", "text/html");
		resp.setBody(html);
		return resp;
	}

	struct stat file_stat;
	if (stat(file_path.c_str(), &file_stat) == -1)
		return ErrorResponse(NOT_FOUND);

	if (access(file_path.c_str(), R_OK) != 0)
		return ErrorResponse(FORBIDDEN);

	std::string body;
	if (!ResponseHelpers::readFileToString(file_path, body))
		return ErrorResponse(INTERNAL_SERVER_ERROR);

	resp.setStatus(OK);
	resp.setHeader("Content-Type", ResponseHelpers::mimeTypeForPath(file_path));
	resp.setBody(body);
	return resp;
}

Response StaticHandler::handleHEAD() {
	Response resp;
	std::string file_path;
	bool autoindex = false;

	if (!ResponseHelpers::resolveTargetPath(request, location, file_path, autoindex))
		return ErrorResponse(NOT_FOUND);

	if (needsDirSlash(autoindex))
		return redirectToDirSlash();

	if (autoindex) {
		std::string html = ResponseHelpers::buildAutoindexHtml(file_path, request.getPath());
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
	resp.setHeader("Content-Type", ResponseHelpers::mimeTypeForPath(file_path));
	resp.setHeader("Content-Length", oss.str());
	return resp;
}

Response StaticHandler::handlePOST(){
	Response resp;
	size_t body_size = request.getBody().size();
	if (body_size > static_cast<size_t>(location.getClientMaxBodySize()))
		return ErrorResponse(PAYLOAD_TOO_LARGE);

	/* O location aceita POST no methods, mas nao tem upload_path: nao ha
	onde gravar. Isso e config dizendo "aqui nao se escreve", nao falha do
	servidor, entao 405 e nao 500. */
	if (location.getUploadPath().empty())
		return ErrorResponse(METHOD_NOT_ALLOWED);

	std::string file_name = ResponseHelpers::extractFileName(request.getPath());
	if (file_name.empty())
		return ErrorResponse(BAD_REQUEST);

	std::string target_path = ResponseHelpers::joinPath(location.getUploadPath(), file_name);
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

Response StaticHandler::handleDELETE() {
	Response resp;
	std::string file_path = ResponseHelpers::resolveUriPath(location, request.getPath());

	struct stat file_stat;
	if (stat(file_path.c_str(), &file_stat) == -1)
		return ErrorResponse(NOT_FOUND);

	if (S_ISDIR(file_stat.st_mode))
		return ErrorResponse(METHOD_NOT_ALLOWED);

	if (unlink(file_path.c_str()) == 0)
		resp.setStatus(NO_CONTENT);
	else
		return ErrorResponse(INTERNAL_SERVER_ERROR);

	return resp;
}

Response StaticHandler::handleMethodAllowed() {
	return ErrorResponse(METHOD_NOT_ALLOWED);
}

Response StaticHandler::build() {
	std::string method = request.getMethod();

	if (!isMethodAllowed(method))
		return handleMethodAllowed();

	if (method == "HEAD") return handleHEAD();
	if (method == "GET") return handleGET();
	if (method == "POST") return handlePOST();
	if (method == "DELETE") return handleDELETE();

	return handleMethodAllowed();
}

bool	StaticHandler::handle(const HttpRequest& req, const Location& loc, Connection& conn) {
	(void)req;
	(void)loc;

	Response resp = build();

	/* O erro volta pra Connection so como código, não como pagina pronta.
	Quem monta a página é o ErrorResponse, que conhece o error_page da
	config. Assim um 404 do estático e um 404 do CGI saem identicos. */
	if (resp.getStatus() >= 400)
		conn.buildErrorResponse(resp.getStatus());
	else
		conn.sendResponse(resp);

	return true;
}
