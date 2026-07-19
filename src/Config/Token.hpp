#ifndef TOKEN_HPP
# define TOKEN_HPP

#include <string>
#include <cstddef>

enum TokenType {
	UNKNOWN,
	DIRECTIVE,
	STRING,
	PATH,
	SYMBOL,
	EDGE_CASE,
	END_OF_STREAM
};

class Token {
	public:
		TokenType	type;
		std::string	value;
		size_t		line;
		size_t		col;

		Token();
		Token(TokenType type, const std::string& value, size_t line, size_t col);
};

#endif
