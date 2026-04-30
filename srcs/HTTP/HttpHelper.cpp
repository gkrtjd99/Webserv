#include "HttpHelper.hpp"

#include <cctype>

namespace HttpHelper
{
	std::string toLowerString(const std::string& s)
	{
		std::string result;

		result.reserve(s.size());
		for(std::size_t i = 0; i < s.size(); ++i)
		{
			result += static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
		}

		return result;
	}

	std::string trim(const std::string& s)
	{
		std::size_t begin = 0;
		std::size_t end = s.size();

		while(begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin])))
		{
			begin++;
		}

		while(end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
		{
			end--;
		}

		return s.substr(begin, end - begin);
	}

}
