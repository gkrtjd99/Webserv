#ifndef CGIFDREGISTRY_HPP
# define CGIFDREGISTRY_HPP

#include "CgiExecutor.hpp"

#include <map>
#include <poll.h>
#include <vector>

class CgiFdRegistry
{
public:
	void registerFds(int clientFd, const CgiExecutor& cgi);
	void unregisterClient(int clientFd);
	void appendPollFds(std::vector<struct pollfd>& pollFds) const;
	bool inputOwner(int fd, int& clientFd) const;
	bool outputOwner(int fd, int& clientFd) const;
	void removeInputFd(int fd);
	void removeOutputFd(int fd);

private:
	std::map<int, int> _inputToClient;
	std::map<int, int> _outputToClient;
};

#endif
