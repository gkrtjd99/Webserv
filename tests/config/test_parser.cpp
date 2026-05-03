#include "ConfigParser.hpp"
#include "ConfigError.hpp"
#include "HttpMethod.hpp"
#include "../test_helpers.hpp"

#include <map>
#include <string>

namespace {

Config parse(const std::string& src)
{
	ConfigParser p("");
	p.parseSource(src, "in.conf");
	return p.result();
}

bool throwsSyntax(const std::string& src, ConfigError& outErr)
{
	try {
		ConfigParser p("");
		p.parseSource(src, "in.conf");
	} catch (const ConfigError& e) {
		outErr = e;
		return e.category() == ConfigError::SYNTAX;
	}
	return false;
}

void test_t_parse_1_minimal()
{
	const std::string src =
		"server { listen 8080; location / { root ./www; } }";
	Config cfg = parse(src);

	EXPECT_EQ(cfg.servers.size(), static_cast<std::size_t>(1));
	const ServerConfig& s = cfg.servers[0];
	EXPECT_EQ(s.port, 8080);
	EXPECT_EQ(s.host, std::string());
	EXPECT_EQ(s.locations.size(), static_cast<std::size_t>(1));
	const LocationConfig& l = s.locations[0];
	EXPECT_EQ(l.path, std::string("/"));
	EXPECT_EQ(l.root, std::string("./www"));
}

void test_t_parse_2_missing_semicolon()
{
	const std::string src =
		"server {\n"
		"    listen 8080\n"
		"    location / { root ./www; }\n"
		"}\n";
	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsSyntax(src, err);

	EXPECT_TRUE(threw);
	EXPECT_EQ(err.category(), ConfigError::SYNTAX);
}

void test_t_parse_3_unknown_directive()
{
	const std::string src = "server { foo bar; }";
	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsSyntax(src, err);

	EXPECT_TRUE(threw);
	EXPECT_CONTAINS(err.what(), "unknown directive 'foo'");
}

void test_t_parse_4_location_outside_server()
{
	const std::string src = "location / { root ./www; }";
	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsSyntax(src, err);

	EXPECT_TRUE(threw);
	EXPECT_CONTAINS(err.what(), "top level");
}

void test_t_parse_5_duplicate_root()
{
	const std::string src =
		"server { listen 80; location / { root ./www; root ./other; } }";
	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsSyntax(src, err);

	EXPECT_TRUE(threw);
	EXPECT_CONTAINS(err.what(), "'root'");
	EXPECT_CONTAINS(err.what(), "duplicated");
}

void test_t_parse_6_unmatched_brace()
{
	const std::string src = "server { listen 8080;";
	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsSyntax(src, err);

	EXPECT_TRUE(threw);
	EXPECT_CONTAINS(err.what(), "end of file");
}

void test_listen_with_host_port()
{
	Config cfg = parse(
		"server { listen 127.0.0.1:9000; location / { root ./w; } }");

	EXPECT_EQ(cfg.servers[0].host, std::string("127.0.0.1"));
	EXPECT_EQ(cfg.servers[0].port, 9000);
}

void test_methods_cgi_return()
{
	Config cfg = parse(
		"server { listen 80;"
		"  location /api {"
		"    root ./www;"
		"    methods GET POST;"
		"    cgi .py /usr/bin/python3;"
		"  }"
		"  location /r {"
		"    return 301 /api/;"
		"  }"
		"}");

	const LocationConfig& api = cfg.servers[0].locations[0];
	EXPECT_EQ(api.methods.size(), static_cast<std::size_t>(2));
	EXPECT_TRUE(api.methods.count(HTTP_GET) == 1);
	EXPECT_TRUE(api.methods.count(HTTP_POST) == 1);
	EXPECT_EQ(api.cgi.size(), static_cast<std::size_t>(1));
	std::map<std::string, std::string>::const_iterator cgiIt = api.cgi.find(".py");
	EXPECT_TRUE(cgiIt != api.cgi.end());
	if (cgiIt != api.cgi.end()) {
		EXPECT_EQ(cgiIt->second, std::string("/usr/bin/python3"));
	}

	const LocationConfig& r = cfg.servers[0].locations[1];
	EXPECT_EQ(r.redirect.first, 301);
	EXPECT_EQ(r.redirect.second, std::string("/api/"));
}

void test_error_page_multi_code()
{
	Config cfg = parse(
		"server { listen 80;"
		"  error_page 404 500 /errors/x.html;"
		"  location / { root ./w; }"
		"}");

	const ServerConfig& s = cfg.servers[0];
	EXPECT_EQ(s.errorPages.size(), static_cast<std::size_t>(2));
	std::map<int, std::string>::const_iterator e404 = s.errorPages.find(404);
	std::map<int, std::string>::const_iterator e500 = s.errorPages.find(500);
	EXPECT_TRUE(e404 != s.errorPages.end());
	EXPECT_TRUE(e500 != s.errorPages.end());
	if (e404 != s.errorPages.end()) {
		EXPECT_EQ(e404->second, std::string("/errors/x.html"));
	}
	if (e500 != s.errorPages.end()) {
		EXPECT_EQ(e500->second, std::string("/errors/x.html"));
	}
}

void test_size_suffix()
{
	Config cfg = parse(
		"server { listen 80; client_max_body_size 2m;"
		"  location / { root ./w; client_max_body_size 512k; }"
		"}");

	EXPECT_EQ(cfg.servers[0].clientMaxBodySize,
			  static_cast<std::size_t>(2 * 1024 * 1024));
	EXPECT_EQ(cfg.servers[0].locations[0].clientMaxBodySize,
			  static_cast<std::size_t>(512 * 1024));
}

void test_autoindex_invalid_value()
{
	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsSyntax(
		"server { listen 80; location / { root ./w; autoindex maybe; } }",
		err);

	EXPECT_TRUE(threw);
	EXPECT_CONTAINS(err.what(), "autoindex");
}

void test_unsupported_method()
{
	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsSyntax(
		"server { listen 80; location / { root ./w; methods PUT; } }",
		err);

	EXPECT_TRUE(threw);
	EXPECT_CONTAINS(err.what(), "PUT");
}

void test_cgi_extension_must_start_with_dot()
{
	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsSyntax(
		"server { listen 80; location / { root ./w; cgi py /usr/bin/python3; } }",
		err);

	EXPECT_TRUE(threw);
	EXPECT_CONTAINS(err.what(), "cgi extension");
}

}

int main()
{
	test_t_parse_1_minimal();
	test_t_parse_2_missing_semicolon();
	test_t_parse_3_unknown_directive();
	test_t_parse_4_location_outside_server();
	test_t_parse_5_duplicate_root();
	test_t_parse_6_unmatched_brace();
	test_listen_with_host_port();
	test_methods_cgi_return();
	test_error_page_multi_code();
	test_size_suffix();
	test_autoindex_invalid_value();
	test_unsupported_method();
	test_cgi_extension_must_start_with_dot();
	return webserv_tests::summarize("test_parser");
}
