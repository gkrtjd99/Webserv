#include "ConfigValidator.hpp"

#include "ConfigError.hpp"
#include "HttpMethod.hpp"

#include <cstddef>
#include <cstdlib>
#include <set>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

const std::size_t kClientMaxBodyLimit = 512u * 1024u * 1024u;

bool startsWith(const std::string& s, const std::string& prefix)
{
	return s.size() >= prefix.size()
		&& s.compare(0, prefix.size(), prefix) == 0;
}

}

ConfigValidator::ConfigValidator(Config& cfg)
	: cfg_(cfg)
{
}

void ConfigValidator::run()
{
	applyDefaults();
	validateGlobal();
	for (std::size_t i = 0; i < cfg_.servers.size(); ++i) {
		validateServer(cfg_.servers[i]);
	}
}

void ConfigValidator::applyDefaults()
{
	for (std::size_t i = 0; i < cfg_.servers.size(); ++i) {
		applyServerDefaults(cfg_.servers[i]);
	}
	for (std::size_t i = 0; i < cfg_.servers.size(); ++i) {
		ServerConfig& s = cfg_.servers[i];
		for (std::size_t j = 0; j < s.locations.size(); ++j) {
			applyLocationDefaults(s, s.locations[j]);
		}
	}
}

void ConfigValidator::applyServerDefaults(ServerConfig& s)
{
	if (s.host.empty()) {
		s.host = "0.0.0.0";
	}
	// V-S-5: explicit 0 ("0 바이트만 허용") 과 미설정을 구분.
	// 미설정인 경우에만 디폴트 1m 으로 채운다.
	if (!s.clientMaxBodySizeSet) {
		s.clientMaxBodySize = 1024u * 1024u;
	}
}

void ConfigValidator::applyLocationDefaults(const ServerConfig& s,
											LocationConfig& l)
{
	if (l.path.size() > 1 && l.path[l.path.size() - 1] == '/') {
		l.path.erase(l.path.size() - 1);
	}
	if (l.index.empty()) {
		l.index = "index.html";
	}
	if (l.methods.empty()) {
		l.methods.insert(HTTP_GET);
	}
	if (l.clientMaxBodySize == 0) {
		l.clientMaxBodySize = s.clientMaxBodySize;
	}
}

void ConfigValidator::validateGlobal()
{
	if (cfg_.servers.empty()) {
		fail("config has no server block");    // V-G-1
	}

	// V-G-2: same (host, port) → server_names must be disjoint.
	for (std::size_t i = 0; i < cfg_.servers.size(); ++i) {
		const ServerConfig& a = cfg_.servers[i];
		for (std::size_t j = i + 1; j < cfg_.servers.size(); ++j) {
			const ServerConfig& b = cfg_.servers[j];
			if (a.host != b.host || a.port != b.port) {
				continue;
			}
			std::set<std::string> bset(b.serverNames.begin(),
									   b.serverNames.end());
			for (std::size_t k = 0; k < a.serverNames.size(); ++k) {
				if (bset.find(a.serverNames[k]) != bset.end()) {
					std::ostringstream oss;
					oss << "server_name '" << a.serverNames[k]
						<< "' duplicated for " << a.host << ":" << a.port;
					fail(oss.str());
				}
			}
		}
	}
}

void ConfigValidator::validateServer(ServerConfig& s)
{
	if (s.port == 0) {
		fail("server block missing 'listen' directive");    // V-S-1
	}
	if (s.port < 1 || s.port > 65535) {
		std::ostringstream oss;
		oss << "port " << s.port << " out of range (1..65535)";
		fail(oss.str());                                    // V-S-2
	}
	// V-S-3: host (default applied so non-empty here).
	if (!isValidIPv4(s.host) && s.host != "localhost"
		&& !isValidHostname(s.host)) {
		fail(std::string("invalid host '") + s.host + "'");
	}
	// V-S-4: server_name pattern.
	for (std::size_t i = 0; i < s.serverNames.size(); ++i) {
		if (!isValidServerName(s.serverNames[i])) {
			fail(std::string("invalid server_name '")
				+ s.serverNames[i] + "'");
		}
	}
	// V-S-5: clientMaxBodySize 0 OK (means literal 0 bytes after defaulting,
	// but defaulting fills 0 → 1m so this is mostly defensive).
	if (s.clientMaxBodySize > kClientMaxBodyLimit) {
		fail("client_max_body_size exceeds 512m limit");
	}
	// V-S-6, V-S-7: errorPages.
	for (std::map<int, std::string>::const_iterator it = s.errorPages.begin();
		it != s.errorPages.end(); ++it) {
		if (it->first < 300 || it->first > 599) {
			std::ostringstream oss;
			oss << "error_page code " << it->first
				<< " out of range (300..599)";
			fail(oss.str());
		}
		if (it->second.empty() || it->second[0] != '/') {
			fail(std::string("error_page path must start with '/', got '")
				+ it->second + "'");
		}
	}

	for (std::size_t i = 0; i < s.locations.size(); ++i) {
		validateLocation(s, s.locations[i]);
	}
}

void ConfigValidator::validateLocation(const ServerConfig& /*s*/,
									LocationConfig& l)
{
	// V-L-1
	if (l.path.empty() || l.path[0] != '/') {
		fail(std::string("location path must start with '/', got '")
			+ l.path + "'");
	}
	// V-L-4
	const bool hasRedirect = (l.redirect.first != 0);
	if (l.root.empty() && !hasRedirect) {
		fail(std::string("location '") + l.path
			+ "' requires 'root' or 'return'");
	}
	// V-L-6 (defensive — defaulting fills GET).
	if (l.methods.empty()) {
		fail(std::string("location '") + l.path + "' has empty methods");
	}
	// V-L-9
	if (hasRedirect && !isValidRedirectCode(l.redirect.first)) {
		std::ostringstream oss;
		oss << "redirect code " << l.redirect.first
			<< " not allowed (301/302/303/307/308 only)";
		fail(oss.str());
	}
	// V-L-10
	if (hasRedirect && !isValidRedirectTarget(l.redirect.second)) {
		fail(std::string("redirect target '") + l.redirect.second
			+ "' must start with '/' or 'http://' or 'https://'");
	}
	// V-L-12
	if (!l.uploadStore.empty()
		&& l.methods.find(HTTP_POST) == l.methods.end()) {
		fail(std::string("location '") + l.path
			+ "' has upload_store but POST is not in methods");
	}
	// V-L-13
	for (std::map<std::string, std::string>::const_iterator it
			= l.cgi.begin(); it != l.cgi.end(); ++it) {
		if (it->first.empty() || it->first[0] != '.') {
			fail(std::string("cgi extension '") + it->first
				+ "' must start with '.'");
		}
		if (it->second.empty() || it->second[0] != '/') {
			fail(std::string("cgi interpreter '") + it->second
				+ "' must be an absolute path");
		}
	}
	// Filesystem checks: V-L-5, V-L-11, V-L-14.
	validateFilesystem(l);
}

void ConfigValidator::validateFilesystem(const LocationConfig& l)
{
	if (!l.root.empty() && !dirExistsReadable(l.root)) {
		fail(std::string("root '") + l.root
			+ "' does not exist or is not readable");
	}
	if (!l.uploadStore.empty() && !dirExistsWritable(l.uploadStore)) {
		fail(std::string("upload_store '") + l.uploadStore
			+ "' does not exist or is not writable");
	}
	for (std::map<std::string, std::string>::const_iterator it
			= l.cgi.begin(); it != l.cgi.end(); ++it) {
		if (!fileExecutable(it->second)) {
			fail(std::string("cgi interpreter '") + it->second
				+ "' is not executable");
		}
	}
}

bool ConfigValidator::isValidIPv4(const std::string& s)
{
	int dots = 0;
	int octet = 0;
	int digits = 0;
	for (std::size_t i = 0; i < s.size(); ++i) {
		const char c = s[i];
		if (c == '.') {
			if (digits == 0 || octet > 255) return false;
			++dots;
			octet = 0;
			digits = 0;
			continue;
		}
		if (c < '0' || c > '9') return false;
		octet = octet * 10 + (c - '0');
		++digits;
		if (digits > 3) return false;
	}
	return dots == 3 && digits > 0 && octet <= 255;
}

bool ConfigValidator::isValidHostname(const std::string& s)
{
	if (s.empty()) return false;
	for (std::size_t i = 0; i < s.size(); ++i) {
		const char c = s[i];
		const bool ok = (c >= 'A' && c <= 'Z')
					 || (c >= 'a' && c <= 'z')
					 || (c >= '0' && c <= '9')
					 || c == '.' || c == '-';
		if (!ok) return false;
	}
	return true;
}

bool ConfigValidator::isValidServerName(const std::string& s)
{
	if (s.empty()) return false;
	for (std::size_t i = 0; i < s.size(); ++i) {
		const char c = s[i];
		const bool ok = (c >= 'A' && c <= 'Z')
					 || (c >= 'a' && c <= 'z')
					 || (c >= '0' && c <= '9')
					 || c == '.' || c == '-';
		if (!ok) return false;
	}
	return true;
}

bool ConfigValidator::isValidRedirectCode(int code)
{
	return code == 301 || code == 302 || code == 303
		|| code == 307 || code == 308;
}

bool ConfigValidator::isValidRedirectTarget(const std::string& s)
{
	if (s.empty()) return false;
	if (s[0] == '/') return true;
	if (startsWith(s, "http://"))  return true;
	if (startsWith(s, "https://")) return true;
	return false;
}

bool ConfigValidator::dirExistsReadable(const std::string& path)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0) return false;
	if (!S_ISDIR(st.st_mode)) return false;
	return access(path.c_str(), R_OK | X_OK) == 0;
}

bool ConfigValidator::dirExistsWritable(const std::string& path)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0) return false;
	if (!S_ISDIR(st.st_mode)) return false;
	return access(path.c_str(), W_OK | X_OK) == 0;
}

bool ConfigValidator::fileExecutable(const std::string& path)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0) return false;
	if (!S_ISREG(st.st_mode)) return false;
	return access(path.c_str(), X_OK) == 0;
}

void ConfigValidator::fail(const std::string& msg)
{
	throw ConfigError(ConfigError::VALIDATION,
					  std::string(), 0, 0, msg);
}
