#include "ConfigError.hpp"

#include <sstream>

namespace {

const char* categoryLabel(ConfigError::Category cat)
{
	switch (cat) {
		case ConfigError::LEX:
			return "lex";
		case ConfigError::SYNTAX:
			return "syntax";
		case ConfigError::VALIDATION:
			return "validation";
	}
	return "unknown";
}

std::string formatMessage(ConfigError::Category cat,
						const std::string& file,
						int line,
						int col,
						const std::string& msg)
{
	std::ostringstream oss;
	oss << "config " << categoryLabel(cat) << " error: "
		<< file << ":" << line << ":" << col << ": " << msg;
	return oss.str();
}

}

ConfigError::ConfigError(Category cat,
						const std::string& file,
						int line,
						int col,
						const std::string& msg)
	: std::runtime_error(formatMessage(cat, file, line, col, msg))
	, cat_(cat)
	, file_(file)
	, line_(line)
	, col_(col)
{
}

ConfigError::~ConfigError() throw() {}

ConfigError::Category ConfigError::category() const { return cat_; }
const std::string&    ConfigError::file() const     { return file_; }
int                   ConfigError::line() const     { return line_; }
int                   ConfigError::col() const      { return col_; }
