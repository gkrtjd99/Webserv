#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include "HttpParser.hpp"

#include <cstddef>
#include <string>

class Connection
{
public:
	enum State
	{
		READING,
		WRITING
	};

	Connection();

	State state() const;
	HttpParser& parser();
	void setWriteBuffer(const std::string& data);

	const char* pendingWriteData() const;
	std::size_t pendingWriteSize() const;

	void consumeWritten(std::size_t size);
	bool writeComplete() const;

private:
	State _state;
	HttpParser _parser;
	std::string _writeBuffer;
	std::size_t _writeOffset;
};

#endif
