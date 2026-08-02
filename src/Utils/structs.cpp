#include "structs.hpp"

Listen::Listen() : host(), port(0) {}

Listen::Listen(const std::string& host, int port)
: host(host), port(port) {}

Location::Location() : path(), root(), redir(), autoindex(false), allow_methods(0), client_max_body__size(0) {}

Location::Location(const std::string& path, const std::string& root, std::vector<std::string> redir, bool autoindex, size_t allow_methods, long long client_max_body__size)
: path(path), root(root), redir(redir), autoindex(autoindex), allow_methods(allow_methods), client_max_body__size(client_max_body__size) {}

Token::Token() : type(UNKNOWN), value(), line(0), col(0) {}

Token::Token(TokenType type, const std::string& value, size_t line, size_t col)
: type(type), value(value), line(line), col(col) {}
