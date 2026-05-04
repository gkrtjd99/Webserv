#include "ConfigError.hpp"
#include "../test_helpers.hpp"

#include <sstream>
#include <string>

namespace {

void test_constructor_stores_fields()
{
	ConfigError err(ConfigError::SYNTAX, "foo.conf", 3, 7, "unexpected token");

	EXPECT_EQ(err.category(), ConfigError::SYNTAX);
	EXPECT_EQ(err.file(), std::string("foo.conf"));
	EXPECT_EQ(err.line(), 3);
	EXPECT_EQ(err.col(), 7);
}

void test_what_uses_standard_format()
{
	ConfigError err(ConfigError::VALIDATION, "default.conf", 12, 1,
					"duplicate listen port");

	const std::string what = err.what();

	EXPECT_CONTAINS(what, "config validation error");
	EXPECT_CONTAINS(what, "default.conf:12:1");
	EXPECT_CONTAINS(what, "duplicate listen port");
}

}

int main()
{
	test_constructor_stores_fields();
	test_what_uses_standard_format();
	return webserv_tests::summarize("test_error");
}
