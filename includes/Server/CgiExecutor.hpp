#ifndef CGIEXECUTOR_HPP
# define CGIEXECUTOR_HPP

#include "Config.hpp"
#include "HttpRequest.hpp"

#include <cstddef>
#include <ctime>
#include <string>
#include <sys/types.h>

struct CgiMatch
{
	std::string extension;
	std::string interpreter;
	std::string scriptFilename;
	std::string scriptName;
	std::string pathInfo;
};

class CgiExecutor
{
public:
	enum State
	{
		NOT_STARTED,
		RUNNING,
		FINISHED,
		FAILED
	};

	CgiExecutor();
	CgiExecutor(const CgiExecutor& other);
	CgiExecutor& operator=(const CgiExecutor& other);
	~CgiExecutor();

	bool start(const HttpRequest& request,
			const ServerConfig& server,
			const LocationConfig& location,
			const CgiMatch& match,
			const std::string& remoteAddress);

	State state() const;
	int inputFd() const;
	int outputFd() const;
	pid_t pid() const;
	int errorStatus() const;

	bool wantsInputWrite() const;
	bool wantsOutputRead() const;
	bool writeInput();
	bool readOutput();
	bool timedOut(std::time_t now) const;
	void cleanup();
	std::string buildHttpResponse(bool keepAlive) const;

private:
	State _state;
	int _inputFd;
	int _outputFd;
	pid_t _pid;
	std::string _input;
	std::size_t _inputOffset;
	std::string _output;
	std::time_t _startedAt;
	int _errorStatus;
	bool _childReaped;

	void resetInactive();
	bool fail(int status);
	void closeInput();
	void closeOutput();
	void killChild();
	void reapChild();
};

#endif
