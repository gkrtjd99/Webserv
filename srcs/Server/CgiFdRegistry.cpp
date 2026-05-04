#include "CgiFdRegistry.hpp"

void CgiFdRegistry::registerFds(int clientFd, const CgiExecutor& cgi)
{
	if(cgi.wantsInputWrite())
	{
		_inputToClient[cgi.inputFd()] = clientFd;
	}
	if(cgi.wantsOutputRead())
	{
		_outputToClient[cgi.outputFd()] = clientFd;
	}
}

void CgiFdRegistry::unregisterClient(int clientFd)
{
	for(std::map<int, int>::iterator it = _inputToClient.begin();
			it != _inputToClient.end();)
	{
		if(it->second == clientFd)
		{
			_inputToClient.erase(it++);
		}
		else
		{
			++it;
		}
	}
	for(std::map<int, int>::iterator it = _outputToClient.begin();
			it != _outputToClient.end();)
	{
		if(it->second == clientFd)
		{
			_outputToClient.erase(it++);
		}
		else
		{
			++it;
		}
	}
}

void CgiFdRegistry::appendPollFds(
		std::vector<struct pollfd>& pollFds) const
{
	struct pollfd pfd;

	for(std::map<int, int>::const_iterator it = _inputToClient.begin();
			it != _inputToClient.end(); ++it)
	{
		pfd.fd = it->first;
		pfd.events = POLLOUT;
		pfd.revents = 0;
		pollFds.push_back(pfd);
	}
	for(std::map<int, int>::const_iterator it = _outputToClient.begin();
			it != _outputToClient.end(); ++it)
	{
		pfd.fd = it->first;
		pfd.events = POLLIN;
		pfd.revents = 0;
		pollFds.push_back(pfd);
	}
}

bool CgiFdRegistry::inputOwner(int fd, int& clientFd) const
{
	std::map<int, int>::const_iterator it = _inputToClient.find(fd);

	if(it == _inputToClient.end())
	{
		return false;
	}
	clientFd = it->second;
	return true;
}

bool CgiFdRegistry::outputOwner(int fd, int& clientFd) const
{
	std::map<int, int>::const_iterator it = _outputToClient.find(fd);

	if(it == _outputToClient.end())
	{
		return false;
	}
	clientFd = it->second;
	return true;
}

void CgiFdRegistry::removeInputFd(int fd)
{
	_inputToClient.erase(fd);
}

void CgiFdRegistry::removeOutputFd(int fd)
{
	_outputToClient.erase(fd);
}
