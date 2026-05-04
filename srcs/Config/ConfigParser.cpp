#include "ConfigParser.hpp"

#include "ConfigError.hpp"
#include "HttpMethod.hpp"

#include <climits>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace {

// lex_ 포인터를 새 lexer 로 잠시 교체했다가 스코프 종료 시 원상 복구.
// throw 가 발생해도 dangling pointer 가 남지 않도록 RAII 로 관리한다.
class LexerScope {
public:
	LexerScope(ConfigLexer*& slot, ConfigLexer* newLex)
		: slot_(slot), prev_(slot)
	{
		slot_ = newLex;
	}
	~LexerScope()
	{
		slot_ = prev_;
	}
private:
	ConfigLexer*& slot_;
	ConfigLexer*  prev_;
	LexerScope(const LexerScope&);
	LexerScope& operator=(const LexerScope&);
};

// includeStack_ 에 push 하고 스코프 종료 시 pop. throw 시 stack 누수 방지.
class IncludeStackScope {
public:
	IncludeStackScope(std::vector<std::string>& stack,
					const std::string& path)
		: stack_(stack)
	{
		stack_.push_back(path);
	}
	~IncludeStackScope()
	{
		stack_.pop_back();
	}
private:
	std::vector<std::string>& stack_;
	IncludeStackScope(const IncludeStackScope&);
	IncludeStackScope& operator=(const IncludeStackScope&);
};

}    // anonymous namespace

ConfigParser::ConfigParser(const std::string& rootPath)
	: result_()
	, rootPath_(rootPath)
	, includeStack_()
	, lex_(0)
{
}

Config ConfigParser::parse()
{
	// 같은 인스턴스로 재호출되어도 깨끗한 결과를 반환하도록 초기화한다.
	result_ = Config();
	includeStack_.clear();

	std::ifstream ifs(rootPath_.c_str());
	if (!ifs.is_open()) {
		throw ConfigError(ConfigError::SYNTAX, rootPath_, 0, 0,
						  "cannot open config file");
	}
	std::ostringstream oss;
	oss << ifs.rdbuf();

	char buf[PATH_MAX];
	std::string canonical = (realpath(rootPath_.c_str(), buf) != 0)
							? std::string(buf) : rootPath_;

	IncludeStackScope frame(includeStack_, canonical);
	parseSource(oss.str(), canonical);
	return result_;
}

void ConfigParser::parseSource(const std::string& source,
								const std::string& file)
{
	ConfigLexer lex(source, file);
	LexerScope scope(lex_, &lex);
	parseTopBody(false);
}

const Config& ConfigParser::result() const
{
	return result_;
}

void ConfigParser::parseTopBody(bool fromInclude)
{
	for (;;) {
		Token tok = lex_->peek();
		if (tok.type == ConfigLexer::TOKEN_EOF) {
			return;
		}
		if (tok.type == ConfigLexer::TOKEN_IDENT && tok.value == "server") {
			parseServerBlock();
			continue;
		}
		if (tok.type == ConfigLexer::TOKEN_IDENT && tok.value == "include") {
			parseTopInclude();
			continue;
		}
		(void)fromInclude;
		syntaxError(tok, std::string("unexpected token '") + tok.value
						 + "' at top level");
	}
}

void ConfigParser::parseServerBlock()
{
	consume();    // "server"
	expect(ConfigLexer::TOKEN_LBRACE, "'{' after 'server'");

	ServerConfig server;
	std::set<std::string> seen;
	parseServerBody(server, seen, false);
	result_.servers.push_back(server);
}

void ConfigParser::parseServerBody(ServerConfig& server,
									std::set<std::string>& seen,
									bool fromInclude)
{
	for (;;) {
		Token tok = lex_->peek();
		if (tok.type == ConfigLexer::TOKEN_EOF) {
			if (fromInclude) {
				return;
			}
			syntaxError(tok, "unexpected end of file inside server block");
		}
		if (tok.type == ConfigLexer::TOKEN_RBRACE) {
			if (fromInclude) {
				syntaxError(tok, "unexpected '}' in included file");
			}
			consume();
			return;
		}
		if (tok.type != ConfigLexer::TOKEN_IDENT) {
			syntaxError(tok, std::string("expected directive, got '")
							 + tok.value + "'");
		}

		const std::string name = tok.value;

		if (name == "listen") {
			ensureUnique(seen, tok);
			consume();
			parseListen(server);
		} else if (name == "server_name") {
			ensureUnique(seen, tok);
			consume();
			parseServerName(server);
		} else if (name == "error_page") {
			consume();
			parseErrorPage(server);
		} else if (name == "client_max_body_size") {
			ensureUnique(seen, tok);
			consume();
			parseClientMaxBodySize(server.clientMaxBodySize,
									&server.clientMaxBodySizeSet);
		} else if (name == "location") {
			consume();
			parseLocationBlock(server);
		} else if (name == "include") {
			parseServerInclude(server, seen);
		} else {
			syntaxError(tok, std::string("unknown directive '") + name + "'");
		}
	}
}

void ConfigParser::parseLocationBlock(ServerConfig& server)
{
	Token pathTok = consumeArg("location path");
	expect(ConfigLexer::TOKEN_LBRACE, "'{' after location path");

	LocationConfig loc;
	loc.path = pathTok.value;
	std::set<std::string> seen;
	parseLocationBody(loc, seen, false);
	server.locations.push_back(loc);
}

void ConfigParser::parseLocationBody(LocationConfig& loc,
									std::set<std::string>& seen,
									bool fromInclude)
{
	for (;;) {
		Token tok = lex_->peek();
		if (tok.type == ConfigLexer::TOKEN_EOF) {
			if (fromInclude) {
				return;
			}
			syntaxError(tok, "unexpected end of file inside location block");
		}
		if (tok.type == ConfigLexer::TOKEN_RBRACE) {
			if (fromInclude) {
				syntaxError(tok, "unexpected '}' in included file");
			}
			consume();
			return;
		}
		if (tok.type != ConfigLexer::TOKEN_IDENT) {
			syntaxError(tok, std::string("expected directive, got '")
							 + tok.value + "'");
		}

		const std::string name = tok.value;

		if (name == "root") {
			ensureUnique(seen, tok);
			consume();
			parseRoot(loc);
		} else if (name == "index") {
			ensureUnique(seen, tok);
			consume();
			parseIndex(loc);
		} else if (name == "autoindex") {
			ensureUnique(seen, tok);
			consume();
			parseAutoindex(loc);
		} else if (name == "methods") {
			ensureUnique(seen, tok);
			consume();
			parseMethods(loc);
		} else if (name == "return") {
			ensureUnique(seen, tok);
			consume();
			parseReturn(loc);
		} else if (name == "upload_store") {
			ensureUnique(seen, tok);
			consume();
			parseUploadStore(loc);
		} else if (name == "cgi") {
			consume();
			parseCgi(loc);
		} else if (name == "client_max_body_size") {
			ensureUnique(seen, tok);
			consume();
			parseClientMaxBodySize(loc.clientMaxBodySize, 0);
		} else if (name == "include") {
			parseLocationInclude(loc, seen);
		} else {
			syntaxError(tok, std::string("unknown directive '") + name + "'");
		}
	}
}

void ConfigParser::parseListen(ServerConfig& server)
{
	Token tok = consumeArg("listen value");
	const std::string& v = tok.value;
	const std::size_t colon = v.find(':');

	if (colon == std::string::npos) {
		server.port = parseIntValue(tok);
	} else {
		if (colon == 0) {
			syntaxError(tok,
						std::string("listen value '") + v
						+ "' has empty host before ':'");
		}
		server.host = v.substr(0, colon);
		Token portTok = tok;
		portTok.value = v.substr(colon + 1);
		server.port = parseIntValue(portTok);
	}
	expect(ConfigLexer::TOKEN_SEMI, "';' after listen value");
}

void ConfigParser::parseServerName(ServerConfig& server)
{
	while (lex_->peek().type != ConfigLexer::TOKEN_SEMI) {
		Token tok = consumeArg("server_name argument");
		server.serverNames.push_back(tok.value);
	}
	if (server.serverNames.empty()) {
		syntaxError(lex_->peek(), "server_name requires at least one name");
	}
	consume();    // SEMI
}

void ConfigParser::parseErrorPage(ServerConfig& server)
{
	std::vector<Token> args;
	while (lex_->peek().type != ConfigLexer::TOKEN_SEMI) {
		args.push_back(consumeArg("error_page argument"));
	}
	if (args.size() < 2) {
		syntaxError(lex_->peek(),
					"error_page requires <code> [code...] <path>");
	}
	consume();    // SEMI

	const Token pathTok = args.back();
	if (pathTok.type == ConfigLexer::TOKEN_NUMBER) {
		syntaxError(pathTok,
					std::string("error_page path expected, got number '")
					+ pathTok.value + "'");
	}
	for (std::size_t i = 0; i + 1 < args.size(); ++i) {
		if (args[i].type != ConfigLexer::TOKEN_NUMBER) {
			syntaxError(args[i],
						std::string("error_page code must be a number, got '")
						+ args[i].value + "'");
		}
		const int code = parseIntValue(args[i]);
		server.errorPages[code] = pathTok.value;
	}
}

void ConfigParser::parseClientMaxBodySize(std::size_t& target, bool* targetSet)
{
	Token tok = consumeArg("client_max_body_size value");
	target = parseSizeValue(tok);
	if (targetSet != 0) {
		*targetSet = true;
	}
	expect(ConfigLexer::TOKEN_SEMI, "';' after client_max_body_size");
}

void ConfigParser::parseRoot(LocationConfig& loc)
{
	Token tok = consumeArg("root path");
	loc.root = tok.value;
	if (loc.root.size() > 1 && loc.root[loc.root.size() - 1] == '/') {
		loc.root.erase(loc.root.size() - 1);
	}
	expect(ConfigLexer::TOKEN_SEMI, "';' after root");
}

void ConfigParser::parseIndex(LocationConfig& loc)
{
	bool first = true;
	while (lex_->peek().type != ConfigLexer::TOKEN_SEMI) {
		Token tok = consumeArg("index name");
		if (first) {
			loc.index = tok.value;
			first = false;
		}
	}
	if (first) {
		syntaxError(lex_->peek(), "index requires at least one name");
	}
	consume();    // SEMI
}

void ConfigParser::parseAutoindex(LocationConfig& loc)
{
	Token tok = consumeArg("autoindex value");
	if (tok.value == "on") {
		loc.autoindex = true;
	} else if (tok.value == "off") {
		loc.autoindex = false;
	} else {
		syntaxError(tok, std::string("autoindex must be 'on' or 'off', got '")
						 + tok.value + "'");
	}
	expect(ConfigLexer::TOKEN_SEMI, "';' after autoindex");
}

void ConfigParser::parseMethods(LocationConfig& loc)
{
	while (lex_->peek().type != ConfigLexer::TOKEN_SEMI) {
		Token tok = consumeArg("method name");
		const HttpMethod m = parseHttpMethod(tok.value);
		if (m == HTTP_UNKNOWN) {
			syntaxError(tok, std::string("unsupported method '") + tok.value
							 + "' (allowed: GET, POST, DELETE)");
		}
		loc.methods.insert(m);
	}
	if (loc.methods.empty()) {
		syntaxError(lex_->peek(), "methods requires at least one method");
	}
	consume();    // SEMI
}

void ConfigParser::parseReturn(LocationConfig& loc)
{
	Token codeTok = consumeArg("return code");
	if (codeTok.type != ConfigLexer::TOKEN_NUMBER) {
		syntaxError(codeTok,
					std::string("return code must be a number, got '")
					+ codeTok.value + "'");
	}
	const int code = parseIntValue(codeTok);
	Token targetTok = consumeArg("return target");
	loc.redirect = std::make_pair(code, targetTok.value);
	expect(ConfigLexer::TOKEN_SEMI, "';' after return");
}

void ConfigParser::parseUploadStore(LocationConfig& loc)
{
	Token tok = consumeArg("upload_store path");
	loc.uploadStore = tok.value;
	expect(ConfigLexer::TOKEN_SEMI, "';' after upload_store");
}

void ConfigParser::parseCgi(LocationConfig& loc)
{
	Token extTok = consumeArg("cgi extension");
	if (extTok.value.empty() || extTok.value[0] != '.') {
		syntaxError(extTok,
					std::string("cgi extension must start with '.', got '")
					+ extTok.value + "'");
	}
	Token interpTok = consumeArg("cgi interpreter");
	loc.cgi[extTok.value] = interpTok.value;
	expect(ConfigLexer::TOKEN_SEMI, "';' after cgi");
}

ConfigParser::Token ConfigParser::consume()
{
	return lex_->next();
}

ConfigParser::Token ConfigParser::consumeArg(const char* what)
{
	Token tok = lex_->next();
	switch (tok.type) {
		case ConfigLexer::TOKEN_LBRACE:
		case ConfigLexer::TOKEN_RBRACE:
		case ConfigLexer::TOKEN_SEMI:
		case ConfigLexer::TOKEN_EOF:
			syntaxError(tok, std::string("expected ") + what);
		default:
			break;
	}
	return tok;
}

void ConfigParser::expect(TokenType type, const char* what)
{
	Token tok = lex_->next();
	if (tok.type != type) {
		syntaxError(tok, std::string("expected ") + what);
	}
}

void ConfigParser::ensureUnique(std::set<std::string>& seen,
								const Token& nameTok)
{
	if (seen.find(nameTok.value) != seen.end()) {
		syntaxError(nameTok,
					std::string("directive '") + nameTok.value
					+ "' duplicated in this block");
	}
	seen.insert(nameTok.value);
}

int ConfigParser::parseIntValue(const Token& tok)
{
	if (tok.value.empty()) {
		syntaxError(tok, "expected integer, got empty value");
	}
	const long kIntMax = static_cast<long>(INT_MAX);
	long n = 0;
	for (std::size_t i = 0; i < tok.value.size(); ++i) {
		const char c = tok.value[i];
		if (c < '0' || c > '9') {
			syntaxError(tok, std::string("expected integer, got '")
							 + tok.value + "'");
		}
		n = n * 10 + (c - '0');
		if (n > kIntMax) {
			syntaxError(tok, std::string("integer value '") + tok.value
							 + "' too large");
		}
	}
	return static_cast<int>(n);
}

std::size_t ConfigParser::parseSizeValue(const Token& tok)
{
	if (tok.value.empty()) {
		syntaxError(tok, "expected size value, got empty value");
	}
	std::size_t end = tok.value.size();
	std::size_t multiplier = 1;
	const char last = tok.value[end - 1];
	if (last == 'k' || last == 'K') {
		multiplier = 1024;
		--end;
	} else if (last == 'm' || last == 'M') {
		multiplier = 1024 * 1024;
		--end;
	}
	if (end == 0) {
		syntaxError(tok, std::string("invalid size value '") + tok.value + "'");
	}

	const std::size_t kSizeMax = static_cast<std::size_t>(-1);
	std::size_t bytes = 0;
	for (std::size_t i = 0; i < end; ++i) {
		const char c = tok.value[i];
		if (c < '0' || c > '9') {
			syntaxError(tok, std::string("invalid size value '") + tok.value
							 + "'");
		}
		const std::size_t digit = static_cast<std::size_t>(c - '0');
		if (bytes > (kSizeMax - digit) / 10) {
			syntaxError(tok, std::string("size value '") + tok.value
							 + "' too large");
		}
		bytes = bytes * 10 + digit;
	}
	if (multiplier != 1 && bytes > kSizeMax / multiplier) {
		syntaxError(tok, std::string("size value '") + tok.value
						 + "' too large");
	}
	return bytes * multiplier;
}

void ConfigParser::syntaxError(const Token& at, const std::string& msg) const
{
	throw ConfigError(ConfigError::SYNTAX, at.file, at.line, at.col, msg);
}

ConfigParser::Token ConfigParser::readIncludeArgs()
{
	consume();    // "include"
	Token pathTok = consumeArg("include path");
	expect(ConfigLexer::TOKEN_SEMI, "';' after include");
	return pathTok;
}

void ConfigParser::parseTopInclude()
{
	Token pathTok = readIncludeArgs();
	ensureDepth(pathTok);

	const std::string resolved  = resolveIncludePath(pathTok.value);
	const std::string canonical = canonicalizePath(resolved, pathTok);
	ensureNoCycle(canonical, pathTok);
	const std::string source = readFileToString(canonical, pathTok);

	IncludeStackScope frame(includeStack_, canonical);
	ConfigLexer sublex(source, canonical);
	LexerScope scope(lex_, &sublex);
	parseTopBody(true);
}

void ConfigParser::parseServerInclude(ServerConfig& server,
									std::set<std::string>& seen)
{
	Token pathTok = readIncludeArgs();
	ensureDepth(pathTok);

	const std::string resolved  = resolveIncludePath(pathTok.value);
	const std::string canonical = canonicalizePath(resolved, pathTok);
	ensureNoCycle(canonical, pathTok);
	const std::string source = readFileToString(canonical, pathTok);

	IncludeStackScope frame(includeStack_, canonical);
	ConfigLexer sublex(source, canonical);
	LexerScope scope(lex_, &sublex);
	parseServerBody(server, seen, true);
}

void ConfigParser::parseLocationInclude(LocationConfig& loc,
										std::set<std::string>& seen)
{
	Token pathTok = readIncludeArgs();
	ensureDepth(pathTok);

	const std::string resolved  = resolveIncludePath(pathTok.value);
	const std::string canonical = canonicalizePath(resolved, pathTok);
	ensureNoCycle(canonical, pathTok);
	const std::string source = readFileToString(canonical, pathTok);

	IncludeStackScope frame(includeStack_, canonical);
	ConfigLexer sublex(source, canonical);
	LexerScope scope(lex_, &sublex);
	parseLocationBody(loc, seen, true);
}

std::string ConfigParser::resolveIncludePath(const std::string& argPath) const
{
	if (!argPath.empty() && argPath[0] == '/') {
		return argPath;
	}
	const std::string currentFile = includeStack_.empty()
									? rootPath_
									: includeStack_.back();
	const std::size_t slash = currentFile.find_last_of('/');
	const std::string dir = (slash == std::string::npos)
							? std::string(".")
							: currentFile.substr(0, slash);
	return dir + "/" + argPath;
}

std::string ConfigParser::canonicalizePath(const std::string& path,
										const Token& at) const
{
	char buf[PATH_MAX];
	if (realpath(path.c_str(), buf) == 0) {
		throw ConfigError(ConfigError::SYNTAX, at.file, at.line, at.col,
						std::string("cannot open included file '") + path
						+ "'");
	}
	return std::string(buf);
}

std::string ConfigParser::readFileToString(const std::string& path,
										const Token& at) const
{
	std::ifstream ifs(path.c_str());
	if (!ifs.is_open()) {
		throw ConfigError(ConfigError::SYNTAX, at.file, at.line, at.col,
						std::string("cannot open included file '") + path
						+ "'");
	}
	std::ostringstream oss;
	oss << ifs.rdbuf();
	return oss.str();
}

void ConfigParser::ensureNoCycle(const std::string& canonical, const Token& at)
{
	for (std::size_t i = 0; i < includeStack_.size(); ++i) {
		if (includeStack_[i] == canonical) {
			throw ConfigError(ConfigError::SYNTAX, at.file, at.line, at.col,
						std::string("include cycle detected at '") + canonical
						+ "'");
		}
	}
}

void ConfigParser::ensureDepth(const Token& at)
{
	if (includeStack_.size() >= kMaxIncludeDepth) {
		throw ConfigError(ConfigError::SYNTAX, at.file, at.line, at.col,
					std::string("include depth limit exceeded"));
	}
}
