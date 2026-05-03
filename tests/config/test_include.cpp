#include "ConfigParser.hpp"
#include "ConfigError.hpp"
#include "../test_helpers.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {

class TempDir {
public:
	TempDir()
	{
		char tmpl[] = "/tmp/webserv-include-test-XXXXXX";
		const char* dir = mkdtemp(tmpl);
		if (dir == 0) {
			std::abort();
		}
		path_ = dir;
	}
	~TempDir()
	{
		const std::string cmd = std::string("rm -rf ") + path_;
		int rc = std::system(cmd.c_str());
		(void)rc;
	}
	const std::string& path() const { return path_; }
private:
	std::string path_;
};

void writeFile(const std::string& path, const std::string& content)
{
	std::ofstream ofs(path.c_str());
	ofs << content;
	ofs.close();
}

bool throwsSyntax(const std::string& rootPath, ConfigError& outErr)
{
	try {
		ConfigParser p(rootPath);
		p.parse();
	} catch (const ConfigError& e) {
		outErr = e;
		return e.category() == ConfigError::SYNTAX;
	}
	return false;
}

void test_t_parse_7_include_merges_two_files()
{
	TempDir tmp;
	writeFile(tmp.path() + "/base.conf",
		"server { listen 8080; include extra.conf; }");
	writeFile(tmp.path() + "/extra.conf",
		"location / { root ./www; }\n"
		"location /api { root ./api; methods GET POST; }");

	ConfigParser p(tmp.path() + "/base.conf");
	Config cfg = p.parse();

	EXPECT_EQ(cfg.servers.size(), static_cast<std::size_t>(1));
	const ServerConfig& s = cfg.servers[0];
	EXPECT_EQ(s.port, 8080);
	EXPECT_EQ(s.locations.size(), static_cast<std::size_t>(2));
	EXPECT_EQ(s.locations[0].path, std::string("/"));
	EXPECT_EQ(s.locations[1].path, std::string("/api"));
}

void test_t_parse_8_self_include_is_cycle()
{
	TempDir tmp;
	const std::string root = tmp.path() + "/loop.conf";
	writeFile(root, "include loop.conf;");

	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsSyntax(root, err);

	EXPECT_TRUE(threw);
	EXPECT_CONTAINS(err.what(), "cycle");
}

void test_t_parse_9_include_depth_exceeded()
{
	TempDir tmp;
	// root → 1 → 2 → ... → 9 (총 9 단계 nested include).
	// kMaxIncludeDepth=8 이라서 9번째 include 진입 시점에 depth limit 발생.
	const int chainLen = 9;
	for (int i = chainLen - 1; i >= 1; --i) {
		std::ostringstream path;
		std::ostringstream content;
		path << tmp.path() << "/d" << i << ".conf";
		content << "include d" << (i + 1) << ".conf;";
		writeFile(path.str(), content.str());
	}
	// 마지막 파일은 빈 내용 (실제로 깊이 초과로 도달 못함).
	std::ostringstream lastPath;
	lastPath << tmp.path() << "/d" << chainLen << ".conf";
	writeFile(lastPath.str(), "");
	const std::string root = tmp.path() + "/d1.conf";

	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsSyntax(root, err);

	EXPECT_TRUE(threw);
	EXPECT_CONTAINS(err.what(), "depth");
}

void test_include_inside_location()
{
	TempDir tmp;
	writeFile(tmp.path() + "/main.conf",
		"server { listen 80; location / { include locopt.conf; } }");
	writeFile(tmp.path() + "/locopt.conf",
		"root ./www; methods GET POST;");

	ConfigParser p(tmp.path() + "/main.conf");
	Config cfg = p.parse();

	const LocationConfig& loc = cfg.servers[0].locations[0];
	EXPECT_EQ(loc.root, std::string("./www"));
	EXPECT_EQ(loc.methods.size(), static_cast<std::size_t>(2));
}

void test_include_state_restored_after_throw()
{
	TempDir tmp;
	const std::string root = tmp.path() + "/loop.conf";
	writeFile(root, "include loop.conf;");

	ConfigParser p(root);
	bool firstThrew = false;
	try {
		p.parse();
	} catch (const ConfigError&) {
		firstThrew = true;
	}
	EXPECT_TRUE(firstThrew);

	// 상태가 RAII 로 정리되었으면 두 번째 호출도 동일 동작.
	bool secondThrew = false;
	try {
		p.parse();
	} catch (const ConfigError& e) {
		secondThrew = true;
		EXPECT_CONTAINS(e.what(), "cycle");
	}
	EXPECT_TRUE(secondThrew);
}

void test_include_missing_file()
{
	TempDir tmp;
	writeFile(tmp.path() + "/main.conf",
		"include does-not-exist.conf;");

	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsSyntax(tmp.path() + "/main.conf", err);

	EXPECT_TRUE(threw);
	EXPECT_CONTAINS(err.what(), "cannot open");
}

}

int main()
{
	test_t_parse_7_include_merges_two_files();
	test_t_parse_8_self_include_is_cycle();
	test_t_parse_9_include_depth_exceeded();
	test_include_inside_location();
	test_include_state_restored_after_throw();
	test_include_missing_file();
	return webserv_tests::summarize("test_include");
}
