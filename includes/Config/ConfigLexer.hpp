#ifndef WEBSERV_CONFIG_CONFIG_LEXER_HPP
#define WEBSERV_CONFIG_CONFIG_LEXER_HPP

#include <string>

class ConfigLexer {
public:
	enum TokenType {
		TOKEN_IDENT,
		TOKEN_STRING,
		TOKEN_NUMBER,
		TOKEN_LBRACE,
		TOKEN_RBRACE,
		TOKEN_SEMI,
		TOKEN_EOF
	};

	struct Token {
		TokenType   type;
		std::string value;
		std::string file;
		int         line;
		int         col;

		Token();
	};

	ConfigLexer(const std::string& source, const std::string& file);

	Token next();
	Token peek();

private:
	std::string source_;
	std::string file_;
	std::size_t pos_;
	int         line_;
	int         col_;
	bool        hasCached_;
	Token       cached_;

	Token scan();
	Token makeToken(TokenType type,
					const std::string& value,
					int line,
					int col) const;

	Token scanQuoted(int startLine, int startCol);
	Token scanBare(int startLine, int startCol);

	void  advance();
	bool  eof() const;
	char  current() const;
	void  skipWhitespace();
	void  rejectNonAscii(char c, int line, int col) const;

	static bool isWhitespaceChar(char c);
	static bool isDelimiter(char c);
	static bool isDigit(char c);
	static bool isIdentStart(char c);
	static bool isIdentCont(char c);
	static bool isAllDigits(const std::string& s);
	static bool isIdent(const std::string& s);
};

#endif
