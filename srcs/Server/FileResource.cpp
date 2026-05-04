#include "FileResource.hpp"

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

bool FileResource::readRegularFile(const std::string& path,
		std::string& body) const
{
	int fd = open(path.c_str(), O_RDONLY);
	char buffer[4096];

	if(fd < 0)
	{
		return false;
	}
	body.clear();
	while(true)
	{
		ssize_t n = read(fd, buffer, sizeof(buffer));

		if(n < 0)
		{
			close(fd);
			return false;
		}
		if(n == 0)
		{
			break;
		}
		body.append(buffer, static_cast<std::size_t>(n));
	}
	close(fd);
	return true;
}

bool FileResource::removeRegularFile(const std::string& path) const
{
	return std::remove(path.c_str()) == 0;
}
