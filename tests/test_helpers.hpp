#ifndef WEBSERV_TESTS_TEST_HELPERS_HPP
#define WEBSERV_TESTS_TEST_HELPERS_HPP

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace webserv_tests {

inline int& failureCount()
{
	static int count = 0;
	return count;
}

inline void reportFailure(const char* file, int line, const std::string& msg)
{
	++failureCount();
	std::cerr << file << ":" << line << ": FAIL " << msg << "\n";
}

inline int summarize(const char* suite)
{
	const int failed = failureCount();
	if (failed == 0) {
		std::cout << "[ OK ] " << suite << "\n";
		return EXIT_SUCCESS;
	}
	std::cerr << "[FAIL] " << suite << " (" << failed << " failures)\n";
	return EXIT_FAILURE;
}

}

#define EXPECT_TRUE(cond)                                                     \
	do {                                                                      \
		if (!(cond)) {                                                        \
			webserv_tests::reportFailure(__FILE__, __LINE__,                  \
										std::string("EXPECT_TRUE(") +         \
										#cond + ")");                         \
		}                                                                     \
	} while (0)

#define EXPECT_EQ(actual, expected)                                           \
	do {                                                                      \
		if (!((actual) == (expected))) {                                      \
			std::ostringstream _oss;                                          \
			_oss << "EXPECT_EQ(" << #actual << ", " << #expected              \
				 << ") got=" << (actual) << " want=" << (expected);           \
			webserv_tests::reportFailure(__FILE__, __LINE__, _oss.str());     \
		}                                                                     \
	} while (0)

#define EXPECT_CONTAINS(haystack, needle)                                     \
	do {                                                                      \
		const std::string _h = (haystack);                                    \
		const std::string _n = (needle);                                      \
		if (_h.find(_n) == std::string::npos) {                               \
			webserv_tests::reportFailure(__FILE__, __LINE__,                  \
										std::string("EXPECT_CONTAINS: '") +   \
										_h + "' missing '" + _n + "'");       \
		}                                                                     \
	} while (0)

#endif
