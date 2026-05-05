#ifndef POLLFDBUILDER_HPP
# define POLLFDBUILDER_HPP

#include "CgiFdRegistry.hpp"
#include "Connection.hpp"
#include "ListenSocketManager.hpp"

#include <map>
#include <poll.h>
#include <vector>

class PollFdBuilder
{
public:
	void build(const ListenSocketManager& listenSockets,
			const std::map<int, Connection>& connections,
			const CgiFdRegistry& cgiFds,
			std::vector<struct pollfd>& pollFds) const;

private:
	void appendConnections(const std::map<int, Connection>& connections,
			std::vector<struct pollfd>& pollFds) const;
};

#endif
