#include "ConfigLexer.hpp"
#include "ConfigParseError.hpp"
#include "UtilsConfig.hpp"
#include <fstream>
#include <cctype>

static bool	is_directive_char(unsigned char c) {
	if (std::isalpha(c)) return true;
	if (std::isdigit(c)) return true;
	if (c == '_') return true;

	return false;
}

ConfigLexer::ConfigLexer() : tokens() {}

ConfigLexer::~ConfigLexer() {}

const std::vector<Token>&	ConfigLexer::getTokens() const {
	return tokens;
}

size_t	ConfigLexer::consumeDirective(const std::string& line, size_t count_line, size_t col) {
	if (col >= line.size())
		return col;

	unsigned char	c = static_cast<unsigned char>(line[col]);
	size_t		start = col;

	if (!is_directive_char(c))
		return col;
	++col;

	while (col < line.size()) {
		c = static_cast<unsigned char>(line[col]);

		if (UtilsConfig::has_char(" \t{;}", c))
			break ;

		if (c == '\'' || c == '"')
			throw ConfigParseError("Invalid quotes in directive", count_line, col, line);

		if (!is_directive_char(c))
			break ;

		++col;
	}

	if (col == start)
		return col;

	tokens.push_back(Token(DIRECTIVE, line.substr(start, col - start), count_line, start + 1));

	return col;
}

size_t	ConfigLexer::consumeName(const std::string& line, size_t count_line, size_t col) {
	if (col >= line.size())
		return col;

	size_t		start = col;

	while (col < line.size()) {
		unsigned char	c = static_cast<unsigned char>(line[col]);

		if (UtilsConfig::has_char(" \t{;}", c))
			break ;

		if (c == '\'' || c == '"')
			throw ConfigParseError("Invalid quotes in name", count_line, col, line);

		if (c < 32 || c == 127)
			throw ConfigParseError("Unexpected control character in name", count_line, col, line);

		++col;
	}

	tokens.push_back(Token(STRING, line.substr(start, col - start), count_line, start + 1));

	return col;
}

size_t	ConfigLexer::consumeString(const std::string& line, size_t count_line, size_t col) {
	if (col >= line.size())
		return col;

	unsigned char	quote = static_cast<unsigned char>(line[col]);
	if (quote != '"' && quote != '\'')
		return col;

	size_t		start = col;
	++col;

	std::string content;
	while (col < line.size()) {
		unsigned char	c = static_cast<unsigned char>(line[col]);

		if (c == '\\') {
			++col;
			if (col >= line.size())
				throw ConfigParseError("Invalid escape at the end of line", count_line, col, line);
			content.push_back(line[col]);
			++col;
			continue ;
		}

		if (c == quote) {
			++col;

			if (content.empty())
				throw ConfigParseError("Empty string", count_line, col, line);

			tokens.push_back(Token(STRING, content, count_line, start + 1));
			return col;
		}
		content.push_back(line[col]);
		++col;
	}

	throw ConfigParseError("Unclosed quotes", count_line, col, line);
}

size_t	ConfigLexer::consumePath(const std::string& line, size_t count_line, size_t col) {
	if (col >= line.size())
		return col;

	unsigned char	c = static_cast<unsigned char>(line[col]);
	size_t		start = col;

	if (UtilsConfig::has_char(" \t{;}", c))
		return col;
	++col;

	while (col < line.size()) {
		c = static_cast<unsigned char>(line[col]);

		if (UtilsConfig::has_char(" \t{;}", c))
			break ;

		if (c == '\'' || c == '"')
			throw ConfigParseError("Invalid quotes in path", count_line, col, line);

		if (c < 32 || c == 127)
			throw ConfigParseError("Unexpected control character in path", count_line, col, line);

		++col;
	}

	tokens.push_back(Token(PATH, line.substr(start, col - start), count_line, start + 1));

	return col;
}

size_t	ConfigLexer::consumeSymbol(const std::string& line, size_t count_line, size_t col) {
	if (col >= line.size())
		return col;

	unsigned char	c = static_cast<unsigned char>(line[col]);
	size_t		start =	col;

	if (!UtilsConfig::has_char("{;}", c))
		return col;
	++col;

	tokens.push_back(Token(SYMBOL, line.substr(start, col - start), count_line, start + 1));

	return col;
}

size_t	ConfigLexer::edgeCase(const std::string& line, size_t count_line, size_t col) {
	if (col >= line.size())
		return col;

	size_t		start = col;

	while (col < line.size()) {
		unsigned char	c = static_cast<unsigned char>(line[col]);

		if (UtilsConfig::has_char(" \t{;}", c))
			break ;

		++col;
	}

	tokens.push_back(Token(EDGE_CASE, line.substr(start, col - start), count_line, start + 1));

	return col;
}

void	ConfigLexer::consumeLine(std::string& line, size_t count_line) {

	if (line.empty())
		return ;

	size_t	col = 0;
	while (col < line.size()) {
		unsigned char c = static_cast<unsigned char>(line[col]);

		if (std::isspace(c)) {
			++col;
			continue ;
		}
		if (col == 0 && (std::isalpha(c) || c == '_'))
			col = consumeDirective(line, count_line, col);
		else if (std::isalpha(c) || c == '_' || std::isdigit(c))
			col = consumeName(line, count_line, col);
		else if (c == '\"' || c == '\'')
			col = consumeString(line, count_line, col);
		else if (c == '/' || c == '.')
			col = consumePath(line, count_line, col);
		else if (c == '{' || c == '}' || c == ';')
			col = consumeSymbol(line, count_line, col);
		else
			col = edgeCase(line, count_line, col);
	}
}

bool	ConfigLexer::tokenize(const char *file) {
	if (!file || !*file)
		return false; // quem chama (Config::load) decide como reportar isso

	std::ifstream	config_file(file);

	if (!config_file)
		return false; // idem -- não abriu, quem chama decide a mensagem

	std::string line;
	size_t	count_lines = 1;
	while (std::getline(config_file, line)) {
		size_t pos = line.find('#');
		if (pos != std::string::npos) {
			line = line.substr(0, pos);
		}
		UtilsConfig::ref_trim(line);
		if (line.length() > 0) {
			consumeLine(line, count_lines);
		}
		++count_lines;
	}
	tokens.push_back(Token(END_OF_STREAM, "EOF", count_lines, 0));
	config_file.close();

	return true;
}
