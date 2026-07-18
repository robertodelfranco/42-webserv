#ifndef CONFIGLEXER_HPP
# define CONFIGLEXER_HPP

#include <string>
#include <vector>
#include "Token.hpp"

// Fase 1 do parsing de config: texto bruto do arquivo vira vector<Token>.
// Não sabe nada sobre Server/Location/diretivas, só reconhece a
// "gramática léxica" (diretiva, string, path, símbolo, etc.).
class ConfigLexer {
	private:
		std::vector<Token>	tokens;

		void	consumeLine(std::string& line, size_t count_line);
		size_t	consumeDirective(const std::string& line, size_t count_line, size_t col);
		size_t	consumeName(const std::string& line, size_t count_line, size_t col);
		size_t	consumeString(const std::string& line, size_t count_line, size_t col);
		size_t	consumePath(const std::string& line, size_t count_line, size_t col);
		size_t	consumeSymbol(const std::string& line, size_t count_line, size_t col);
		size_t	edgeCase(const std::string& line, size_t count_line, size_t col);

	public:
		ConfigLexer();
		~ConfigLexer();

		bool	tokenize(const char *file); // false = arquivo nulo/vazio/não abriu
		const std::vector<Token>&	getTokens() const;
};

#endif
