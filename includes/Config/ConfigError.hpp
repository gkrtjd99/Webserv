#ifndef WEBSERV_CONFIG_CONFIG_ERROR_HPP
#define WEBSERV_CONFIG_CONFIG_ERROR_HPP

#include <stdexcept>
#include <string>

class ConfigError : public std::runtime_error {
public:
	enum Category {
		LEX,
		SYNTAX,
		VALIDATION
	};

	ConfigError(Category cat,
				const std::string& file,
				int line,
				int col,
				const std::string& msg);
	virtual ~ConfigError() throw();

	Category           category() const;
	const std::string& file() const;
	int                line() const;
	int                col() const;

private:
	Category    cat_;
	std::string file_;
	int         line_;
	int         col_;
};

#endif
