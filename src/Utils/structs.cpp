#include "structs.hpp"

Listen::Listen() : host(), port(0) {}

Listen::Listen(const std::string& host, int port)
: host(host), port(port) {}

Location::Location() : path(), root(), allow_methods(), autoindex(false) {}

Location::Location(const std::string& path, const std::string& root, const std::vector<std::string>& allow_methods, bool autoindex)
: path(path), root(root), allow_methods(allow_methods), autoindex(autoindex) {}

Token::Token() : type(UNKNOWN), value(), line(0), col(0) {}

Token::Token(TokenType type, const std::string& value, int line, int col)
: type(type), value(value), line(line), col(col) {}
