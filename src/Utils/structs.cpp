#include "structs.hpp"

Listen::Listen() : host(), port(0) {}

Listen::Listen(const std::string& host, int port)
: host(host), port(port) {}

Location::Location() : path(), root(), index_files(), redir(), autoindex(false), allow_methods(0), client_max_body_size(0) {}

Token::Token() : type(UNKNOWN), value(), line(0), col(0) {}

Token::Token(TokenType type, const std::string& value, size_t line, size_t col)
: type(type), value(value), line(line), col(col) {}
