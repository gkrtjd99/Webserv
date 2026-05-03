#ifndef HTTPSYNTAX_HPP
# define HTTPSYNTAX_HPP

#include "HttpStatus.hpp"

#include <string>

namespace HttpSyntax
{
	int hexValue(char c);
	bool hasWhitespace(const std::string& s);
	bool isTchar(char c);
	bool isToken(const std::string& value);
	bool isHttpVersion(const std::string& value);
	bool isValidHostName(const std::string& value);
	bool hasInvalidFieldValueChar(const std::string& value);
	bool splitFieldLine(const std::string& line,
			std::string& name,
			std::string& value);
	bool isInvalidRawTargetChar(char c);
	bool isInvalidDecodedPathChar(char c);
	bool percentTripletsAreValid(const std::string& value);
	bool percentDecodePath(const std::string& rawPath, std::string& decoded);
	HttpStatus normalizeDecodedPath(const std::string& decoded,
			std::string& normalized);
}

#endif
