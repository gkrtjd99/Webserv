#ifndef HTTPHELPER_HPP
# define HTTPHELPER_HPP

#include <string>

namespace HttpHelper
{
	std::string toLowerString(const std::string& s);
	std::string trim(const std::string& s);
	int hexValue(char c);
	bool hasWhitespace(const std::string& s);
}

#endif
