#ifndef WEBSERV_CONFIG_CONFIG_VALIDATOR_HPP
#define WEBSERV_CONFIG_CONFIG_VALIDATOR_HPP

#include <string>

#include "Config.hpp"

class ConfigValidator {
public:
	explicit ConfigValidator(Config& cfg);

	// 디폴트 채우기 후 의미 검증을 수행한다. 실패 시 ConfigError(VALIDATION).
	void run();

private:
	Config& cfg_;

	void applyDefaults();
	void applyServerDefaults(ServerConfig& s);
	void applyLocationDefaults(const ServerConfig& s, LocationConfig& l);

	void validateGlobal();
	void validateServer(ServerConfig& s);
	void validateLocation(const ServerConfig& s, LocationConfig& l);
	void validateFilesystem(const LocationConfig& l);

	static bool isValidIPv4(const std::string& s);
	static bool isValidHostname(const std::string& s);
	static bool isValidServerName(const std::string& s);
	static bool isValidRedirectCode(int code);
	static bool isValidRedirectTarget(const std::string& s);

	static bool dirExistsReadable(const std::string& path);
	static bool dirExistsWritable(const std::string& path);
	static bool fileExecutable(const std::string& path);

	static void fail(const std::string& msg);
};

#endif
