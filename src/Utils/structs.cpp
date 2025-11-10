#include "structs.hpp"

Listen::Listen() : host(), port(0) {}

Location::Location() : path(), root_override(), allow_methods(), autoindex(false) {}

Token::Token() : type(UNKNOWN), value(), line(0), col(0) {}
