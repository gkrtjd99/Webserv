#include "ConfigLexer.hpp"
#include "ConfigError.hpp"
#include "../test_helpers.hpp"

#include <string>
#include <vector>

namespace {

typedef ConfigLexer::Token Token;
typedef ConfigLexer::TokenType TokenType;

std::vector<Token> drain(const std::string& src, const std::string& file)
{
	ConfigLexer lex(src, file);
	std::vector<Token> out;
	for (;;) {
		Token t = lex.next();
		out.push_back(t);
		if (t.type == ConfigLexer::TOKEN_EOF) {
			break;
		}
	}
	return out;
}

bool throwsLex(const std::string& src, ConfigError& outErr)
{
	try {
		ConfigLexer lex(src, "input.conf");
		for (;;) {
			Token t = lex.next();
			if (t.type == ConfigLexer::TOKEN_EOF) {
				return false;
			}
		}
	} catch (const ConfigError& e) {
		outErr = e;
		return true;
	}
	return false;
}

void test_t_lex_1_basic_server_block()
{
	std::vector<Token> toks = drain("server { listen 8080; }", "in.conf");

	EXPECT_EQ(toks.size(), static_cast<std::size_t>(7));
	EXPECT_EQ(toks[0].type, ConfigLexer::TOKEN_IDENT);
	EXPECT_EQ(toks[0].value, std::string("server"));
	EXPECT_EQ(toks[1].type, ConfigLexer::TOKEN_LBRACE);
	EXPECT_EQ(toks[2].type, ConfigLexer::TOKEN_IDENT);
	EXPECT_EQ(toks[2].value, std::string("listen"));
	EXPECT_EQ(toks[3].type, ConfigLexer::TOKEN_NUMBER);
	EXPECT_EQ(toks[3].value, std::string("8080"));
	EXPECT_EQ(toks[4].type, ConfigLexer::TOKEN_SEMI);
	EXPECT_EQ(toks[5].type, ConfigLexer::TOKEN_RBRACE);
	EXPECT_EQ(toks[6].type, ConfigLexer::TOKEN_EOF);
}

void test_t_lex_2_quoted_string_keeps_spaces()
{
	std::vector<Token> toks = drain("\"a b c\"", "in.conf");

	EXPECT_EQ(toks.size(), static_cast<std::size_t>(2));
	EXPECT_EQ(toks[0].type, ConfigLexer::TOKEN_STRING);
	EXPECT_EQ(toks[0].value, std::string("a b c"));
	EXPECT_EQ(toks[0].line, 1);
	EXPECT_EQ(toks[0].col, 1);
	EXPECT_EQ(toks[1].type, ConfigLexer::TOKEN_EOF);
}

void test_t_lex_3_unterminated_string()
{
	ConfigError err(ConfigError::LEX, "", 0, 0, "");
	const bool threw = throwsLex("\"abc", err);

	EXPECT_TRUE(threw);
	EXPECT_EQ(err.category(), ConfigError::LEX);
	EXPECT_EQ(err.line(), 1);
	EXPECT_EQ(err.col(), 1);
}

void test_t_lex_4_non_ascii_byte()
{
	const std::string src = std::string("listen ") + static_cast<char>(0xC3)
							+ static_cast<char>(0xA9) + ";";
	ConfigError err(ConfigError::LEX, "", 0, 0, "");
	const bool threw = throwsLex(src, err);

	EXPECT_TRUE(threw);
	EXPECT_EQ(err.category(), ConfigError::LEX);
}

void test_t_lex_5_hash_is_bare_string()
{
	std::vector<Token> toks = drain("#hello server{}", "in.conf");

	EXPECT_EQ(toks.size(), static_cast<std::size_t>(5));
	EXPECT_EQ(toks[0].type, ConfigLexer::TOKEN_STRING);
	EXPECT_EQ(toks[0].value, std::string("#hello"));
	EXPECT_EQ(toks[1].type, ConfigLexer::TOKEN_IDENT);
	EXPECT_EQ(toks[1].value, std::string("server"));
	EXPECT_EQ(toks[2].type, ConfigLexer::TOKEN_LBRACE);
	EXPECT_EQ(toks[3].type, ConfigLexer::TOKEN_RBRACE);
	EXPECT_EQ(toks[4].type, ConfigLexer::TOKEN_EOF);
}

void test_t_lex_6_no_whitespace()
{
	std::vector<Token> toks = drain("server{listen 8080;}", "in.conf");

	EXPECT_EQ(toks.size(), static_cast<std::size_t>(7));
	EXPECT_EQ(toks[0].type, ConfigLexer::TOKEN_IDENT);
	EXPECT_EQ(toks[1].type, ConfigLexer::TOKEN_LBRACE);
	EXPECT_EQ(toks[2].type, ConfigLexer::TOKEN_IDENT);
	EXPECT_EQ(toks[3].type, ConfigLexer::TOKEN_NUMBER);
	EXPECT_EQ(toks[4].type, ConfigLexer::TOKEN_SEMI);
	EXPECT_EQ(toks[5].type, ConfigLexer::TOKEN_RBRACE);
	EXPECT_EQ(toks[6].type, ConfigLexer::TOKEN_EOF);
}

void test_position_tracking_across_newlines()
{
	std::vector<Token> toks = drain("server\n  {\n}", "in.conf");

	EXPECT_EQ(toks[0].line, 1);
	EXPECT_EQ(toks[0].col, 1);
	EXPECT_EQ(toks[1].line, 2);
	EXPECT_EQ(toks[1].col, 3);
	EXPECT_EQ(toks[2].line, 3);
	EXPECT_EQ(toks[2].col, 1);
}

void test_peek_does_not_consume()
{
	ConfigLexer lex("listen 8080;", "in.conf");
	Token p1 = lex.peek();
	Token p2 = lex.peek();
	Token n1 = lex.next();

	EXPECT_EQ(p1.type, ConfigLexer::TOKEN_IDENT);
	EXPECT_EQ(p1.value, std::string("listen"));
	EXPECT_EQ(p2.value, std::string("listen"));
	EXPECT_EQ(n1.value, std::string("listen"));
	EXPECT_EQ(lex.next().type, ConfigLexer::TOKEN_NUMBER);
}

void test_quoted_escapes()
{
	std::vector<Token> toks = drain("\"a\\\"b\\\\c\"", "in.conf");

	EXPECT_EQ(toks[0].type, ConfigLexer::TOKEN_STRING);
	EXPECT_EQ(toks[0].value, std::string("a\"b\\c"));
}

void test_invalid_escape_throws()
{
	ConfigError err(ConfigError::LEX, "", 0, 0, "");
	const bool threw = throwsLex("\"\\n\"", err);

	EXPECT_TRUE(threw);
	EXPECT_EQ(err.category(), ConfigError::LEX);
}

}

int main()
{
	test_t_lex_1_basic_server_block();
	test_t_lex_2_quoted_string_keeps_spaces();
	test_t_lex_3_unterminated_string();
	test_t_lex_4_non_ascii_byte();
	test_t_lex_5_hash_is_bare_string();
	test_t_lex_6_no_whitespace();
	test_position_tracking_across_newlines();
	test_peek_does_not_consume();
	test_quoted_escapes();
	test_invalid_escape_throws();
	return webserv_tests::summarize("test_lexer");
}
