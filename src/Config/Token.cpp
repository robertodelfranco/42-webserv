#include "Token.hpp"

Token::Token() : type(UNKNOWN), value(), line(0), col(0) {}

Token::Token(TokenType type, const std::string& value, size_t line, size_t col)
: type(type), value(value), line(line), col(col) {}
