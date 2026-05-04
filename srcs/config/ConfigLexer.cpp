#include "ConfigLexer.hpp"

#include "ConfigError.hpp"

ConfigLexer::Token::Token()
	: type(TOKEN_EOF)
	, value()
	, file()
	, line(0)
	, col(0)
{
}

ConfigLexer::ConfigLexer(const std::string& source, const std::string& file)
	: source_(source)
	, file_(file)
	, pos_(0)
	, line_(1)
	, col_(1)
	, hasCached_(false)
	, cached_()
{
}

ConfigLexer::Token ConfigLexer::next()
{
	if (hasCached_) {
		Token tok = cached_;
		hasCached_ = false;
		return tok;
	}
	return scan();
}

ConfigLexer::Token ConfigLexer::peek()
{
	if (!hasCached_) {
		cached_ = scan();
		hasCached_ = true;
	}
	return cached_;
}

ConfigLexer::Token ConfigLexer::scan()
{
	skipWhitespace();

	if (eof()) {
		return makeToken(TOKEN_EOF, std::string(), line_, col_);
	}

	const int startLine = line_;
	const int startCol  = col_;
	const char c = current();

	rejectNonAscii(c, startLine, startCol);

	if (c == '{') {
		advance();
		return makeToken(TOKEN_LBRACE, "{", startLine, startCol);
	}
	if (c == '}') {
		advance();
		return makeToken(TOKEN_RBRACE, "}", startLine, startCol);
	}
	if (c == ';') {
		advance();
		return makeToken(TOKEN_SEMI, ";", startLine, startCol);
	}
	if (c == '"') {
		return scanQuoted(startLine, startCol);
	}
	return scanBare(startLine, startCol);
}

ConfigLexer::Token ConfigLexer::makeToken(TokenType type,
										  const std::string& value,
										  int line,
										  int col) const
{
	Token tok;
	tok.type  = type;
	tok.value = value;
	tok.file  = file_;
	tok.line  = line;
	tok.col   = col;
	return tok;
}

ConfigLexer::Token ConfigLexer::scanQuoted(int startLine, int startCol)
{
	advance();    // 시작 따옴표 소비
	std::string value;
	while (!eof()) {
		const char c = current();
		rejectNonAscii(c, line_, col_);
		if (c == '"') {
			advance();
			return makeToken(TOKEN_STRING, value, startLine, startCol);
		}
		if (c == '\\') {
			advance();
			if (eof()) {
				throw ConfigError(ConfigError::LEX, file_,
								  startLine, startCol,
								  "unterminated string starting here");
			}
			const char esc = current();
			if (esc != '"' && esc != '\\') {
				throw ConfigError(ConfigError::LEX, file_, line_, col_,
								  "invalid escape sequence");
			}
			value.push_back(esc);
			advance();
			continue;
		}
		value.push_back(c);
		advance();
	}
	throw ConfigError(ConfigError::LEX, file_, startLine, startCol,
					  "unterminated string starting here");
}

ConfigLexer::Token ConfigLexer::scanBare(int startLine, int startCol)
{
	std::string value;
	while (!eof()) {
		const char c = current();
		if (isWhitespaceChar(c) || isDelimiter(c)) {
			break;
		}
		rejectNonAscii(c, line_, col_);
		value.push_back(c);
		advance();
	}

	TokenType type;
	if (isAllDigits(value)) {
		type = TOKEN_NUMBER;
	} else if (isIdent(value)) {
		type = TOKEN_IDENT;
	} else {
		type = TOKEN_STRING;
	}
	return makeToken(type, value, startLine, startCol);
}

void ConfigLexer::advance()
{
	if (eof()) {
		return;
	}
	if (source_[pos_] == '\n') {
		++line_;
		col_ = 1;
	} else {
		++col_;
	}
	++pos_;
}

bool ConfigLexer::eof() const
{
	return pos_ >= source_.size();
}

char ConfigLexer::current() const
{
	return source_[pos_];
}

void ConfigLexer::skipWhitespace()
{
	while (!eof() && isWhitespaceChar(current())) {
		advance();
	}
}

void ConfigLexer::rejectNonAscii(char c, int line, int col) const
{
	const unsigned char uc = static_cast<unsigned char>(c);
	if (uc > 0x7F) {
		throw ConfigError(ConfigError::LEX, file_, line, col,
						  "non-ASCII byte");
	}
}

bool ConfigLexer::isWhitespaceChar(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool ConfigLexer::isDelimiter(char c)
{
	return c == '{' || c == '}' || c == ';';
}

bool ConfigLexer::isDigit(char c)
{
	return c >= '0' && c <= '9';
}

bool ConfigLexer::isIdentStart(char c)
{
	return (c >= 'A' && c <= 'Z')
		|| (c >= 'a' && c <= 'z')
		|| c == '_';
}

bool ConfigLexer::isIdentCont(char c)
{
	return isIdentStart(c) || isDigit(c);
}

bool ConfigLexer::isAllDigits(const std::string& s)
{
	if (s.empty()) {
		return false;
	}
	for (std::size_t i = 0; i < s.size(); ++i) {
		if (!isDigit(s[i])) {
			return false;
		}
	}
	return true;
}

bool ConfigLexer::isIdent(const std::string& s)
{
	if (s.empty() || !isIdentStart(s[0])) {
		return false;
	}
	for (std::size_t i = 1; i < s.size(); ++i) {
		if (!isIdentCont(s[i])) {
			return false;
		}
	}
	return true;
}
