#include "HttpSyntax.hpp"

#include <cctype>
#include <vector>

namespace HttpSyntax
{
	int hexValue(char c)
	{
		if(c >= '0' && c <= '9')
		{
			return c - '0';
		}
		if(c >= 'a' && c <= 'f')
		{
			return c - 'a' + 10;
		}
		if(c >= 'A' && c <= 'F')
		{
			return c - 'A' + 10;
		}
		return -1;
	}

	bool hasWhitespace(const std::string& s)
	{
		for(std::size_t i = 0; i < s.size(); i++)
		{
			if(std::isspace(static_cast<unsigned char>(s[i])))
			{
				return true;
			}
		}
		return false;
	}

	bool isTchar(char c)
	{
		unsigned char uc = static_cast<unsigned char>(c);

		if(std::isalnum(uc))
		{
			return true;
		}
		return c == '!' || c == '#' || c == '$' || c == '%'
			|| c == '&' || c == '\'' || c == '*' || c == '+'
			|| c == '-' || c == '.' || c == '^' || c == '_'
			|| c == '`' || c == '|' || c == '~';
	}

	bool isToken(const std::string& value)
	{
		if(value.empty())
		{
			return false;
		}
		for(std::size_t i = 0; i < value.size(); i++)
		{
			if(!isTchar(value[i]))
			{
				return false;
			}
		}
		return true;
	}

	bool hasInvalidFieldValueChar(const std::string& value)
	{
		for(std::size_t i = 0; i < value.size(); i++)
		{
			unsigned char c = static_cast<unsigned char>(value[i]);

			if((c < 0x20 && c != '\t') || c == 0x7f)
			{
				return true;
			}
		}
		return false;
	}

	bool splitFieldLine(const std::string& line,
			std::string& name,
			std::string& value)
	{
		std::size_t colon = line.find(':');

		if(colon == std::string::npos || colon == 0)
		{
			return false;
		}
		if(line[colon - 1] == ' ' || line[colon - 1] == '\t')
		{
			return false;
		}

		name = line.substr(0, colon);
		value = line.substr(colon + 1);
		return isToken(name) && !hasInvalidFieldValueChar(value);
	}

	bool isInvalidRawTargetChar(char c)
	{
		unsigned char uc = static_cast<unsigned char>(c);

		return uc <= 0x20 || uc == 0x7f;
	}

	bool isInvalidDecodedPathChar(char c)
	{
		unsigned char uc = static_cast<unsigned char>(c);

		return uc < 0x20 || uc == 0x7f;
	}

	bool percentTripletsAreValid(const std::string& value)
	{
		for(std::size_t i = 0; i < value.size(); i++)
		{
			if(isInvalidRawTargetChar(value[i]))
			{
				return false;
			}
			if(value[i] == '%')
			{
				if(i + 2 >= value.size()
						|| hexValue(value[i + 1]) < 0
						|| hexValue(value[i + 2]) < 0)
				{
					return false;
				}
				i += 2;
			}
		}
		return true;
	}

	bool percentDecodePath(const std::string& rawPath, std::string& decoded)
	{
		decoded.clear();
		for(std::size_t i = 0; i < rawPath.size(); i++)
		{
			char c = rawPath[i];

			if(isInvalidRawTargetChar(c))
			{
				return false;
			}
			if(c == '%')
			{
				if(i + 2 >= rawPath.size())
				{
					return false;
				}

				int hi = hexValue(rawPath[i + 1]);
				int lo = hexValue(rawPath[i + 2]);
				if(hi < 0 || lo < 0)
				{
					return false;
				}

				c = static_cast<char>(hi * 16 + lo);
				i += 2;
			}

			if(isInvalidDecodedPathChar(c))
			{
				return false;
			}

			decoded += c;
		}
		return true;
	}

	bool normalizeDecodedPath(const std::string& decoded,
			std::string& normalized)
	{
		std::vector<std::string> segments;
		bool trailingSlash;
		std::size_t begin;

		if(decoded.empty() || decoded[0] != '/')
		{
			return false;
		}

		trailingSlash = decoded.size() > 1 && decoded[decoded.size() - 1] == '/';
		begin = 1;
		while(begin <= decoded.size())
		{
			std::size_t end = decoded.find('/', begin);
			std::string segment;

			if(end == std::string::npos)
			{
				segment = decoded.substr(begin);
				begin = decoded.size() + 1;
			}
			else
			{
				segment = decoded.substr(begin, end - begin);
				begin = end + 1;
			}

			if(segment.empty() || segment == ".")
			{
				continue;
			}
			if(segment == "..")
			{
				if(segments.empty())
				{
					return false;
				}
				segments.pop_back();
				continue;
			}
			segments.push_back(segment);
		}

		normalized = "/";
		for(std::size_t i = 0; i < segments.size(); i++)
		{
			if(i != 0)
			{
				normalized += "/";
			}
			normalized += segments[i];
		}
		if(trailingSlash && normalized != "/")
		{
			normalized += "/";
		}
		return true;
	}
}
