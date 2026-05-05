#ifndef FILERESOURCE_HPP
# define FILERESOURCE_HPP

#include <string>

class FileResource
{
public:
	bool readRegularFile(const std::string& path, std::string& body) const;
	bool removeRegularFile(const std::string& path) const;
};

#endif
