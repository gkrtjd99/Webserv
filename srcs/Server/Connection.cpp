#include "Connection.hpp"

Connection::Connection()
	: _state(READING),
	  _writeOffset(0)
{
}

Connection::State Connection::state() const
{
	return _state;
}

HttpParser& Connection::parser()
{
	return _parser;
}

void Connection::setWriteBuffer(const std::string& data)
{
	_writeBuffer = data;
	_writeOffset = 0;
	_state = WRITING;
}

const char* Connection::pendingWriteData() const
{
	return _writeBuffer.data() + _writeOffset;
}

std::size_t Connection::pendingWriteSize() const
{
	return _writeBuffer.size() - _writeOffset;
}

void Connection::consumeWritten(std::size_t size)
{
	_writeOffset += size;
	if (_writeOffset > _writeBuffer.size())
	{
		_writeOffset = _writeBuffer.size();
	}
}

bool Connection::writeComplete() const
{
	return _writeOffset >= _writeBuffer.size();
}
