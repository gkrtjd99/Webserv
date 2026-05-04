#ifndef CGITYPES_HPP
# define CGITYPES_HPP

#include <string>

struct CgiMatch
{
	std::string extension;
	std::string interpreter;
	std::string scriptFilename;
	std::string scriptName;
	std::string pathInfo;
};

#endif
