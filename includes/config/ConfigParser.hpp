#ifndef WEBSERV_CONFIG_CONFIG_PARSER_HPP
#define WEBSERV_CONFIG_CONFIG_PARSER_HPP

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "Config.hpp"
#include "ConfigLexer.hpp"

class ConfigParser {
public:
	explicit ConfigParser(const std::string& rootPath);

	Config parse();

	// 파일 IO 를 거치지 않고 이미 로드된 소스 텍스트를 파싱한다.
	// 단위 테스트와 추후 include 처리 양쪽에서 사용한다.
	void          parseSource(const std::string& source,
								const std::string& file);
	const Config& result() const;

private:
	typedef ConfigLexer::Token     Token;
	typedef ConfigLexer::TokenType TokenType;

	Config                   result_;
	std::string              rootPath_;
	std::vector<std::string> includeStack_;
	ConfigLexer*             lex_;

	static const std::size_t kMaxIncludeDepth = 8;

	void parseTopBody(bool fromInclude);
	void parseServerBody(ServerConfig& server,
							std::set<std::string>& seen,
							bool fromInclude);
	void parseLocationBody(LocationConfig& loc,
							std::set<std::string>& seen,
							bool fromInclude);

	void parseServerBlock();
	void parseLocationBlock(ServerConfig& server);

	void parseTopInclude();
	void parseServerInclude(ServerConfig& server,
							std::set<std::string>& seen);
	void parseLocationInclude(LocationConfig& loc,
								std::set<std::string>& seen);
	Token readIncludeArgs();

	std::string resolveIncludePath(const std::string& argPath) const;
	std::string canonicalizePath(const std::string& path,
									const Token& at) const;
	std::string readFileToString(const std::string& path,
									const Token& at) const;
	void        ensureNoCycle(const std::string& canonical,
								const Token& at);
	void        ensureDepth(const Token& at);

	void parseListen(ServerConfig& server);
	void parseServerName(ServerConfig& server);
	void parseErrorPage(ServerConfig& server);

	void parseRoot(LocationConfig& loc);
	void parseIndex(LocationConfig& loc);
	void parseAutoindex(LocationConfig& loc);
	void parseMethods(LocationConfig& loc);
	void parseReturn(LocationConfig& loc);
	void parseUploadStore(LocationConfig& loc);
	void parseCgi(LocationConfig& loc);
	void parseClientMaxBodySize(std::size_t& target);

	Token consume();
	Token consumeArg(const char* what);
	void  expect(TokenType type, const char* what);

	void  ensureUnique(std::set<std::string>& seen,
						const Token& nameTok);

	int         parseIntValue(const Token& tok);
	std::size_t parseSizeValue(const Token& tok);

	void syntaxError(const Token& at, const std::string& msg) const;
};

#endif
