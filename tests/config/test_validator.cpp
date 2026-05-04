#include "ConfigValidator.hpp"
#include "ConfigError.hpp"
#include "Config.hpp"
#include "HttpMethod.hpp"
#include "../test_helpers.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

class TempDir {
public:
	TempDir()
	{
		char tmpl[] = "/tmp/webserv-validator-test-XXXXXX";
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

bool runFails(Config& cfg, ConfigError& outErr)
{
	try {
		ConfigValidator v(cfg);
		v.run();
	} catch (const ConfigError& e) {
		outErr = e;
		return e.category() == ConfigError::VALIDATION;
	}
	return false;
}

// 검증을 통과해야 하는 서버 한 개를 만든다.
ServerConfig okServer(const std::string& root)
{
	ServerConfig s;
	s.host = "127.0.0.1";
	s.port = 8080;

	LocationConfig l;
	l.path = "/";
	l.root = root;
	l.methods.insert(HTTP_GET);
	s.locations.push_back(l);
	return s;
}

void test_t_val_1_no_servers()
{
	Config cfg;
	ConfigError err(ConfigError::VALIDATION, "", 0, 0, "");
	EXPECT_TRUE(runFails(cfg, err));
	EXPECT_CONTAINS(err.what(), "no server block");
}

void test_t_val_2_missing_listen()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());
	s.port = 0;    // listen 없음
	cfg.servers.push_back(s);

	ConfigError err(ConfigError::VALIDATION, "", 0, 0, "");
	EXPECT_TRUE(runFails(cfg, err));
	EXPECT_CONTAINS(err.what(), "listen");
}

void test_t_val_3_port_out_of_range()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());
	s.port = 70000;
	cfg.servers.push_back(s);

	ConfigError err(ConfigError::VALIDATION, "", 0, 0, "");
	EXPECT_TRUE(runFails(cfg, err));
	EXPECT_CONTAINS(err.what(), "out of range");
}

void test_t_val_4_body_size_over_limit()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());
	s.setClientMaxBodySize(600u * 1024u * 1024u);    // 600m
	cfg.servers.push_back(s);

	ConfigError err(ConfigError::VALIDATION, "", 0, 0, "");
	EXPECT_TRUE(runFails(cfg, err));
	EXPECT_CONTAINS(err.what(), "512m");
}

void test_t_val_5_error_page_code_out_of_range()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());
	s.errorPages[999] = "/x.html";
	cfg.servers.push_back(s);

	ConfigError err(ConfigError::VALIDATION, "", 0, 0, "");
	EXPECT_TRUE(runFails(cfg, err));
	EXPECT_CONTAINS(err.what(), "error_page code 999");
}

void test_t_val_6_location_path_no_slash()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());
	s.locations[0].path = "abc";
	cfg.servers.push_back(s);

	ConfigError err(ConfigError::VALIDATION, "", 0, 0, "");
	EXPECT_TRUE(runFails(cfg, err));
	EXPECT_CONTAINS(err.what(), "must start with '/'");
}

void test_t_val_7_root_missing_no_redirect()
{
	Config cfg;
	ServerConfig s;
	s.host = "127.0.0.1";
	s.port = 80;

	LocationConfig l;
	l.path = "/";
	l.methods.insert(HTTP_GET);
	s.locations.push_back(l);
	cfg.servers.push_back(s);

	ConfigError err(ConfigError::VALIDATION, "", 0, 0, "");
	EXPECT_TRUE(runFails(cfg, err));
	EXPECT_CONTAINS(err.what(), "'root' or 'return'");
}

void test_t_val_8_root_does_not_exist()
{
	Config cfg;
	ServerConfig s = okServer("/this/path/should/not/exist/_x_");
	cfg.servers.push_back(s);

	ConfigError err(ConfigError::VALIDATION, "", 0, 0, "");
	EXPECT_TRUE(runFails(cfg, err));
	EXPECT_CONTAINS(err.what(), "root");
}

void test_t_val_9_upload_store_no_post()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());
	s.locations[0].uploadStore = tmp.path();    // 존재하는 dir
	// methods 는 GET 만.
	cfg.servers.push_back(s);

	ConfigError err(ConfigError::VALIDATION, "", 0, 0, "");
	EXPECT_TRUE(runFails(cfg, err));
	EXPECT_CONTAINS(err.what(), "POST is not in methods");
}

void test_t_val_10_cgi_interpreter_not_executable()
{
	TempDir tmp;
	const std::string fake = tmp.path() + "/notexec";
	std::ofstream(fake.c_str()) << "#!/bin/sh\n";
	chmod(fake.c_str(), 0644);    // 실행 권한 없음

	Config cfg;
	ServerConfig s = okServer(tmp.path());
	s.locations[0].cgi[".py"] = fake;
	cfg.servers.push_back(s);

	ConfigError err(ConfigError::VALIDATION, "", 0, 0, "");
	EXPECT_TRUE(runFails(cfg, err));
	EXPECT_CONTAINS(err.what(), "not executable");
}

void test_t_val_11_same_host_port_same_server_name()
{
	TempDir tmp;
	Config cfg;
	ServerConfig a = okServer(tmp.path());
	a.serverNames.push_back("api.local");
	ServerConfig b = okServer(tmp.path());
	b.serverNames.push_back("api.local");
	cfg.servers.push_back(a);
	cfg.servers.push_back(b);

	ConfigError err(ConfigError::VALIDATION, "", 0, 0, "");
	EXPECT_TRUE(runFails(cfg, err));
	EXPECT_CONTAINS(err.what(), "api.local");
}

void test_t_val_12_location_body_size_falls_back_to_server()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());
	s.setClientMaxBodySize(2u * 1024u * 1024u);    // 2m
	s.locations[0].clientMaxBodySize = 0;
	cfg.servers.push_back(s);

	ConfigValidator v(cfg);
	v.run();    // 통과해야 함

	EXPECT_EQ(cfg.servers[0].locations[0].clientMaxBodySize,
			static_cast<std::size_t>(2u * 1024u * 1024u));
}

void test_t_val_13_location_index_default()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());
	s.locations[0].index = "";
	cfg.servers.push_back(s);

	ConfigValidator v(cfg);
	v.run();

	EXPECT_EQ(cfg.servers[0].locations[0].index, std::string("index.html"));
}

void test_t_val_14_redirect_code_invalid()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());
	s.locations[0].redirect = std::make_pair(200, std::string("/elsewhere"));
	cfg.servers.push_back(s);

	ConfigError err(ConfigError::VALIDATION, "", 0, 0, "");
	EXPECT_TRUE(runFails(cfg, err));
	EXPECT_CONTAINS(err.what(), "redirect code 200");
}

void test_t_val_15_redirect_target_no_slash()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());
	s.locations[0].redirect = std::make_pair(301,
											std::string("missing-slash"));
	cfg.servers.push_back(s);

	ConfigError err(ConfigError::VALIDATION, "", 0, 0, "");
	EXPECT_TRUE(runFails(cfg, err));
	EXPECT_CONTAINS(err.what(), "missing-slash");
}

void test_server_defaults_host()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());
	s.host = "";
	cfg.servers.push_back(s);

	ConfigValidator v(cfg);
	v.run();
	EXPECT_EQ(cfg.servers[0].host, std::string("0.0.0.0"));
}

void test_server_defaults_body_size_when_unset()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());
	// clientMaxBodySizeSet 은 false (미설정) → 1m 으로 채워져야 함.
	cfg.servers.push_back(s);

	ConfigValidator v(cfg);
	v.run();
	EXPECT_EQ(cfg.servers[0].clientMaxBodySize,
			static_cast<std::size_t>(1024u * 1024u));
}

void test_v_l_3_duplicate_locations_kept_last()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());

	LocationConfig dup;
	dup.path = "/";
	dup.root = tmp.path();
	dup.methods.insert(HTTP_GET);
	dup.index = "alt.html";    // 마지막 유지 여부 확인용 표식
	s.locations.push_back(dup);

	cfg.servers.push_back(s);

	ConfigValidator v(cfg);
	v.run();

	EXPECT_EQ(cfg.servers[0].locations.size(),
			static_cast<std::size_t>(1));
	EXPECT_EQ(cfg.servers[0].locations[0].index,
			std::string("alt.html"));
}

void test_dedupe_three_or_more_locations()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());

	// okServer 가 location["/"] 1개를 만들어 둠. 동일 path 2개 더 추가.
	for (int i = 0; i < 2; ++i) {
		LocationConfig dup;
		dup.path = "/";
		dup.root = tmp.path();
		dup.methods.insert(HTTP_GET);
		std::ostringstream tag;
		tag << "v" << (i + 1) << ".html";
		dup.index = tag.str();
		s.locations.push_back(dup);
	}
	cfg.servers.push_back(s);

	ConfigValidator v(cfg);
	v.run();

	EXPECT_EQ(cfg.servers[0].locations.size(),
			static_cast<std::size_t>(1));
	EXPECT_EQ(cfg.servers[0].locations[0].index,
			std::string("v2.html"));    // 마지막 push 만 유지
}

void test_v_s_8_empty_locations_warning()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s;
	s.host = "127.0.0.1";
	s.port = 8080;
	cfg.servers.push_back(s);    // locations 비어있음

	ConfigValidator v(cfg);
	v.run();    // V-S-8 은 SHOULD → 경고만, 통과해야 함

	EXPECT_EQ(cfg.servers[0].locations.size(),
			static_cast<std::size_t>(0));
}

void test_server_explicit_zero_preserved()
{
	TempDir tmp;
	Config cfg;
	ServerConfig s = okServer(tmp.path());
	s.setClientMaxBodySize(0);    // 사용자가 명시적으로 0 작성
	cfg.servers.push_back(s);

	ConfigValidator v(cfg);
	v.run();
	EXPECT_EQ(cfg.servers[0].clientMaxBodySize,
			static_cast<std::size_t>(0));
}

}

int main()
{
	test_t_val_1_no_servers();
	test_t_val_2_missing_listen();
	test_t_val_3_port_out_of_range();
	test_t_val_4_body_size_over_limit();
	test_t_val_5_error_page_code_out_of_range();
	test_t_val_6_location_path_no_slash();
	test_t_val_7_root_missing_no_redirect();
	test_t_val_8_root_does_not_exist();
	test_t_val_9_upload_store_no_post();
	test_t_val_10_cgi_interpreter_not_executable();
	test_t_val_11_same_host_port_same_server_name();
	test_t_val_12_location_body_size_falls_back_to_server();
	test_t_val_13_location_index_default();
	test_t_val_14_redirect_code_invalid();
	test_t_val_15_redirect_target_no_slash();
	test_server_defaults_host();
	test_server_defaults_body_size_when_unset();
	test_server_explicit_zero_preserved();
	test_v_l_3_duplicate_locations_kept_last();
	test_dedupe_three_or_more_locations();
	test_v_s_8_empty_locations_warning();
	return webserv_tests::summarize("test_validator");
}
