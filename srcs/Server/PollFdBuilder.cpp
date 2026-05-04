#include "PollFdBuilder.hpp"

void PollFdBuilder::build(const ListenSocketManager& listenSockets,
		const std::map<int, Connection>& connections,
		const CgiFdRegistry& cgiFds,
		std::vector<struct pollfd>& pollFds) const
{
	pollFds.clear();
	listenSockets.appendPollFds(pollFds);
	appendConnections(connections, pollFds);
	cgiFds.appendPollFds(pollFds);
}

void PollFdBuilder::appendConnections(
		const std::map<int, Connection>& connections,
		std::vector<struct pollfd>& pollFds) const
{
	struct pollfd pfd;

	for(std::map<int, Connection>::const_iterator it = connections.begin();
			it != connections.end(); ++it)
	{
		pfd.fd = it->first;
		pfd.events = 0;
		if(it->second.state() == Connection::READING)
		{
			pfd.events |= POLLIN;
		}
		if(it->second.state() == Connection::WRITING)
		{
			pfd.events |= POLLOUT;
		}
		if(pfd.events != 0)
		{
			pfd.revents = 0;
			pollFds.push_back(pfd);
		}
	}
}
