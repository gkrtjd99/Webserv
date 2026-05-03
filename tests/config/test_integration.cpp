#include "Config.hpp"
#include "ConfigError.hpp"
#include "HttpMethod.hpp"
#include "../test_helpers.hpp"

#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

class TempDir {
public:
	TempDir()
	{
		char tmpl[] = "/tmp/webserv-integration-test-XXXXXX";
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
}

bool throwsCategory(const std::string& path,
					ConfigError::Category cat,
					ConfigError& outErr)
{
	try {
		Config::parse(path);
	} catch (const ConfigError& e) {
		outErr = e;
		return e.category() == cat;
	}
	return false;
}

void test_t_int_1_minimal_default_conf()
{
	TempDir tmp;
	const std::string conf = tmp.path() + "/default.conf";
	writeFile(conf,
		"server {\n"
		"    listen 8080;\n"
		"    server_name localhost;\n"
		"    location / {\n"
		"        root " + tmp.path() + ";\n"
		"        index index.html;\n"
		"    }\n"
		"}\n");

	Config cfg = Config::parse(conf);

	EXPECT_EQ(cfg.servers.size(), static_cast<std::size_t>(1));
	const ServerConfig& s = cfg.servers[0];
	EXPECT_EQ(s.host, std::string("0.0.0.0"));    // 디폴트 적용
	EXPECT_EQ(s.port, 8080);
	EXPECT_EQ(s.clientMaxBodySize,
			  static_cast<std::size_t>(1024u * 1024u));    // 디폴트
	EXPECT_EQ(s.locations.size(), static_cast<std::size_t>(1));
	EXPECT_EQ(s.locations[0].index, std::string("index.html"));
	EXPECT_TRUE(s.locations[0].methods.count(HTTP_GET) == 1);
}

void test_t_int_2_cgi_conf()
{
	TempDir tmp;
	// /bin/sh 는 항상 실행 가능한 absolute path 라고 가정.
	const std::string conf = tmp.path() + "/cgi.conf";
	writeFile(conf,
		"server {\n"
		"    listen 9090;\n"
		"    location /cgi-bin {\n"
		"        root " + tmp.path() + ";\n"
		"        methods GET POST;\n"
		"        cgi .sh /bin/sh;\n"
		"    }\n"
		"}\n");

	Config cfg = Config::parse(conf);

	const LocationConfig& l = cfg.servers[0].locations[0];
	EXPECT_EQ(l.cgi.size(), static_cast<std::size_t>(1));
	std::map<std::string, std::string>::const_iterator it = l.cgi.find(".sh");
	EXPECT_TRUE(it != l.cgi.end());
	if (it != l.cgi.end()) {
		EXPECT_EQ(it->second, std::string("/bin/sh"));
	}
}

void test_t_int_3_missing_file()
{
	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsCategory("/tmp/__webserv_does_not_exist__.conf",
									  ConfigError::SYNTAX, err);

	EXPECT_TRUE(threw);
	EXPECT_CONTAINS(err.what(), "cannot open");
}

void test_t_int_4_syntax_error_in_file()
{
	TempDir tmp;
	const std::string conf = tmp.path() + "/broken.conf";
	writeFile(conf,
		"server {\n"
		"    listen 8080\n"        // 세미콜론 누락
		"    location / { root " + tmp.path() + "; }\n"
		"}\n");

	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsCategory(conf, ConfigError::SYNTAX, err);

	EXPECT_TRUE(threw);
	EXPECT_EQ(err.category(), ConfigError::SYNTAX);
}

void test_validation_error_propagates()
{
	TempDir tmp;
	const std::string conf = tmp.path() + "/bad.conf";
	writeFile(conf,
		"server {\n"
		"    listen 70000;\n"     // 포트 범위 초과
		"    location / { root " + tmp.path() + "; }\n"
		"}\n");

	ConfigError err(ConfigError::SYNTAX, "", 0, 0, "");
	const bool threw = throwsCategory(conf, ConfigError::VALIDATION, err);

	EXPECT_TRUE(threw);
	EXPECT_EQ(err.category(), ConfigError::VALIDATION);
}

}

int main()
{
	test_t_int_1_minimal_default_conf();
	test_t_int_2_cgi_conf();
	test_t_int_3_missing_file();
	test_t_int_4_syntax_error_in_file();
	test_validation_error_propagates();
	return webserv_tests::summarize("test_integration");
}
