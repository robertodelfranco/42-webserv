#include "CgiHandler.hpp"
#include "../Network/Router.hpp"
#include "../Network/Connection.hpp"
#include "../Cgi/CgiProcess.hpp"
#include "../Utils/Logger.hpp"
#include "../ServerConfig/Location.hpp"
#include "../Request/HttpRequest.hpp"
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <cstdlib>

// "content-type" -> "HTTP_CONTENT_TYPE": maiúscula e '-' vira '_'.
static std::string	toMetaName(const std::string& header) {
	std::string	out = "HTTP_";

	for (size_t i = 0; i < header.size(); ++i) {
		char	c = header[i];
		if (c == '-')
			out += '_';
		else if (c >= 'a' && c <= 'z')
			out += static_cast<char>(c - 'a' + 'A');
		else
			out += c;
	}
	return out;
}

static std::string	toString(size_t n) {
	std::ostringstream	oss;
	oss << n;
	return oss.str();
}

// diretório do script, pro chdir() do filho ("/a/b/x.py" -> "/a/b")
static std::string	dirNameOf(const std::string& path) {
	std::string::size_type	slash = path.find_last_of('/');

	if (slash == std::string::npos)
		return ".";
	if (slash == 0)
		return "/";
	return path.substr(0, slash);
}

std::vector<std::string>	CgiHandler::buildEnv(const HttpRequest& req,
												 Connection& conn,
												 const std::string& scriptPath) {
	std::vector<std::string>	env;

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("SERVER_SOFTWARE=webserv/1.0");
	env.push_back("SERVER_PROTOCOL=" + req.getHTTPVersion());
	env.push_back("REQUEST_METHOD=" + req.getMethod());
	env.push_back("SCRIPT_NAME=" + req.getPath());
	env.push_back("SCRIPT_FILENAME=" + scriptPath);

	/* O cgi_tester oficial da 42 espera PATH_INFO = o path completo da
	request, não a interpretação estrita do RFC (que seria só o que sobra
	depois do nome do script). */
	env.push_back("PATH_INFO=" + req.getPath());
	env.push_back("PATH_TRANSLATED=" + scriptPath);

	// sempre exportada, mesmo vazia: o script conta com ela existindo
	env.push_back("QUERY_STRING=" + req.getQuery());

	// php-cgi se recusa a rodar sem isto (proteção histórica cgi.force_redirect).
	// Pro python é inofensivo, então vai sempre.
	env.push_back("REDIRECT_STATUS=200");

	const std::string&	body = req.getBody();
	if (!body.empty()) {
		// tamanho do body DECODIFICADO: se veio chunked, o parser já desmontou
		env.push_back("CONTENT_LENGTH=" + toString(body.size()));

		const std::string	type = req.getHeader("content-type");
		if (!type.empty())
			env.push_back("CONTENT_TYPE=" + type);
	}

	// SERVER_NAME sai do Host, sem a porta; sem Host, cai pra porta do listener
	std::string	host = req.getHeader("host");
	std::string::size_type	colon = host.find(':');
	if (colon != std::string::npos)
		host = host.substr(0, colon);
	env.push_back("SERVER_NAME=" + (host.empty() ? std::string("localhost") : host));
	env.push_back("SERVER_PORT=" + toString(conn.getServerPort()));

	// todo header vira HTTP_*, menos os dois que já têm nome próprio acima
	const std::map<std::string, std::string>&	headers = req.getHeaders();
	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
		 it != headers.end(); ++it) {
		if (it->first == "content-length" || it->first == "content-type")
			continue;
		env.push_back(toMetaName(it->first) + "=" + it->second);
	}

	/* Daqui pra frente NINGUÉM insere nada neste vector. Quem colhe os
	c_str() é o filho, e um push_back depois disso realocaria as strings,
	deixando todo o envp apontando pra memória morta. */
	return env;
}

bool	CgiHandler::handle(const HttpRequest& req, const Location& loc, Connection& conn) {
	const std::string	scriptPath = Router::resolvePath(loc, req.getPath());

	Logger::info() << "CGI " << req.getMethod() << " " << req.getPath()
		<< " -> " << scriptPath;

	/* Tudo que dá pra saber ANTES de forkar é decidido antes de forkar: um
	fork só pra descobrir que o arquivo não existe é caro e ainda tornaria o
	erro assíncrono, quando ele pode ser respondido agora mesmo. */
	struct stat	st;
	if (stat(scriptPath.c_str(), &st) < 0 || !S_ISREG(st.st_mode)) {
		Logger::warning() << "CGI: script inexistente: " << scriptPath;
		conn.buildErrorResponse(404);
		return true;
	}
	if (access(scriptPath.c_str(), R_OK) < 0) {
		Logger::warning() << "CGI: script sem permissao de leitura: " << scriptPath;
		conn.buildErrorResponse(403);
		return true;
	}

	const std::string&	interpreter = loc.getCgiPath();
	if (interpreter.empty() || access(interpreter.c_str(), X_OK) < 0) {
		// config aponta pra um interpretador que não existe/não executa:
		// é o "gateway" que está quebrado, não o pedido do cliente -> 502
		Logger::error() << "CGI: interpretador invalido: '" << interpreter << "'";
		conn.buildErrorResponse(502);
		return true;
	}

	/* Caminho absoluto porque o filho faz chdir() antes do execve: depois
	disso um caminho relativo apontaria pra outro lugar. */
	char		buf[PATH_MAX];
	std::string	absPath = scriptPath;
	if (realpath(scriptPath.c_str(), buf))
		absPath = buf;

	CgiProcess*	cgi = new CgiProcess(interpreter, absPath, dirNameOf(absPath),
									 buildEnv(req, conn, absPath), req.getBody());

	if (!cgi->start()) {
		delete cgi;
		conn.buildErrorResponse(500); // pipe/fork falhou: problema nosso
		return true;
	}

	/* Este handler morre na linha seguinte ao return. A posse do processo
	passa pro Connection, que vive enquanto o filho viver, e o false avisa
	"não terminei": a conexão entra em CGI_RUNNING e o resto acontece pelo
	poll(), evento a evento. */
	conn.adoptCgiProcess(cgi);
	return false;
}
