#include "CgiExecutor.hpp"

#include <ctime>
#include <unistd.h>

namespace
{
	const int CGI_TIMEOUT_SECONDS = 5;
}

CgiExecutor::CgiExecutor()
	: _state(NOT_STARTED),
	  _process(),
	  _environment(),
	  _responseParser(),
	  _input(),
	  _inputOffset(0),
	  _output(),
	  _startedAt(0),
	  _errorStatus(0)
{
}

CgiExecutor::CgiExecutor(const CgiExecutor& /*other*/)
	: _state(NOT_STARTED),
	  _process(),
	  _environment(),
	  _responseParser(),
	  _input(),
	  _inputOffset(0),
	  _output(),
	  _startedAt(0),
	  _errorStatus(0)
{
}

CgiExecutor& CgiExecutor::operator=(const CgiExecutor& other)
{
	if(this != &other)
	{
		cleanup();
		resetInactive();
	}
	return *this;
}

CgiExecutor::~CgiExecutor()
{
	cleanup();
}

bool CgiExecutor::start(const HttpRequest& request,
		const ServerConfig& server,
		const LocationConfig& /*location*/,
		const CgiMatch& match,
		const std::string& remoteAddress)
{
	std::vector<std::string> envStrings;

	cleanup();
	resetInactive();
	envStrings = _environment.build(request, server, match, remoteAddress);
	if(!_process.start(match.interpreter, match.scriptFilename, envStrings))
	{
		return fail(500);
	}
	_input = request.body();
	_inputOffset = 0;
	_output.clear();
	_startedAt = std::time(NULL);
	_errorStatus = 0;
	_state = RUNNING;
	if(_input.empty())
	{
		_process.closeInput();
	}
	return true;
}

CgiExecutor::State CgiExecutor::state() const
{
	return _state;
}

int CgiExecutor::inputFd() const
{
	return _process.inputFd();
}

int CgiExecutor::outputFd() const
{
	return _process.outputFd();
}

pid_t CgiExecutor::pid() const
{
	return _process.pid();
}

int CgiExecutor::errorStatus() const
{
	return _errorStatus;
}

bool CgiExecutor::wantsInputWrite() const
{
	return _state == RUNNING && _process.inputOpen();
}

bool CgiExecutor::wantsOutputRead() const
{
	return _state == RUNNING && _process.outputOpen();
}

bool CgiExecutor::writeInput()
{
	ssize_t n;

	if(_state != RUNNING || !_process.inputOpen())
	{
		return true;
	}
	if(_inputOffset >= _input.size())
	{
		_process.closeInput();
		return true;
	}
	n = write(_process.inputFd(), _input.data() + _inputOffset,
			_input.size() - _inputOffset);
	if(n > 0)
	{
		_inputOffset += static_cast<std::size_t>(n);
		if(_inputOffset >= _input.size())
		{
			_process.closeInput();
		}
		return true;
	}
	if(n == 0)
	{
		return true;
	}
	return fail(500);
}

bool CgiExecutor::readOutput()
{
	char buffer[4096];
	ssize_t n;

	if(_state != RUNNING || !_process.outputOpen())
	{
		return true;
	}
	n = read(_process.outputFd(), buffer, sizeof(buffer));
	if(n > 0)
	{
		_output.append(buffer, static_cast<std::size_t>(n));
		return true;
	}
	if(n == 0)
	{
		_process.closeOutput();
		_process.reapChild();
		_state = FINISHED;
		return true;
	}
	return fail(502);
}

bool CgiExecutor::timedOut(std::time_t now) const
{
	return _state == RUNNING
		&& _startedAt != 0
		&& now - _startedAt > CGI_TIMEOUT_SECONDS;
}

void CgiExecutor::cleanup()
{
	_process.cleanup();
	resetInactive();
}

std::string CgiExecutor::buildHttpResponse(bool keepAlive) const
{
	return _responseParser.buildHttpResponse(_output, keepAlive);
}

void CgiExecutor::resetInactive()
{
	_state = NOT_STARTED;
	_process.resetInactive();
	_input.clear();
	_inputOffset = 0;
	_output.clear();
	_startedAt = 0;
	_errorStatus = 0;
}

bool CgiExecutor::fail(int status)
{
	_state = FAILED;
	_errorStatus = status;
	_process.cleanup();
	return false;
}
