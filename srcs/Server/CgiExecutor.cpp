#include "CgiExecutor.hpp"

#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <map>
#include <sstream>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace
{
	const int CGI_TIMEOUT_SECONDS = 5;

	std::string toString(std::size_t value)
	{
		std::ostringstream oss;

		oss << value;
		return oss.str();
	}

	std::string toString(int value)
	{
		std::ostringstream oss;

		oss << value;
		return oss.str();
	}

	std::string trim(const std::string& value)
	{
		std::size_t begin = 0;
		std::size_t end = value.size();

		while(begin < value.size()
				&& (value[begin] == ' ' || value[begin] == '\t'))
		{
			begin++;
		}
		while(end > begin && (value[end - 1] == ' '
					|| value[end - 1] == '\t'
					|| value[end - 1] == '\r'))
		{
			end--;
		}
		return value.substr(begin, end - begin);
	}

	std::string toUpperHeaderName(const std::string& value)
	{
		std::string result;

		for(std::size_t i = 0; i < value.size(); i++)
		{
			char c = value[i];

			if(c >= 'a' && c <= 'z')
			{
				c = static_cast<char>(c - 'a' + 'A');
			}
			else if(c == '-')
			{
				c = '_';
			}
			result += c;
		}
		return result;
	}

	std::string toLower(const std::string& value)
	{
		std::string result;

		for(std::size_t i = 0; i < value.size(); i++)
		{
			char c = value[i];

			if(c >= 'A' && c <= 'Z')
			{
				c = static_cast<char>(c - 'A' + 'a');
			}
			result += c;
		}
		return result;
	}

	const char* reasonPhrase(int status)
	{
		switch(status)
		{
		case 200:
			return "OK";
		case 201:
			return "Created";
		case 204:
			return "No Content";
		case 301:
			return "Moved Permanently";
		case 302:
			return "Found";
		case 303:
			return "See Other";
		case 307:
			return "Temporary Redirect";
		case 308:
			return "Permanent Redirect";
		case 400:
			return "Bad Request";
		case 403:
			return "Forbidden";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 413:
			return "Content Too Large";
		case 414:
			return "URI Too Long";
		case 431:
			return "Request Header Fields Too Large";
		case 500:
			return "Internal Server Error";
		case 501:
			return "Not Implemented";
		case 502:
			return "Bad Gateway";
		case 504:
			return "Gateway Timeout";
		case 505:
			return "HTTP Version Not Supported";
		default:
			return "Error";
		}
	}

	std::string buildSimpleResponse(int status, bool keepAlive)
	{
		std::string body = reasonPhrase(status);
		std::ostringstream response;

		body += "\n";
		response << "HTTP/1.1 " << status << " " << reasonPhrase(status)
			<< "\r\n";
		response << "Connection: " << (keepAlive ? "keep-alive" : "close")
			<< "\r\n";
		response << "Content-Length: " << body.size() << "\r\n";
		response << "Content-Type: text/plain\r\n";
		response << "\r\n";
		response << body;
		return response.str();
	}

	std::string directoryName(const std::string& path)
	{
		std::size_t slash = path.find_last_of('/');

		if(slash == std::string::npos)
		{
			return ".";
		}
		if(slash == 0)
		{
			return "/";
		}
		return path.substr(0, slash);
	}

	bool setNonBlocking(int fd)
	{
		int flags = fcntl(fd, F_GETFL, 0);

		if(flags < 0)
		{
			return false;
		}
		return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
	}

	void closeIfOpen(int& fd)
	{
		if(fd != -1)
		{
			close(fd);
			fd = -1;
		}
	}

	void addEnv(std::vector<std::string>& env,
			const std::string& name,
			const std::string& value)
	{
		env.push_back(name + "=" + value);
	}

	std::vector<std::string> buildEnvironment(const HttpRequest& request,
			const ServerConfig& server,
			const CgiMatch& match,
			const std::string& remoteAddress)
	{
		std::vector<std::string> env;
		std::string contentType = request.header("content-type");
		const std::map<std::string, std::string>& headers = request.headers();

		addEnv(env, "GATEWAY_INTERFACE", "CGI/1.1");
		addEnv(env, "SERVER_PROTOCOL", request.version());
		addEnv(env, "REQUEST_METHOD", request.methodString());
		addEnv(env, "SCRIPT_FILENAME", match.scriptFilename);
		addEnv(env, "SCRIPT_NAME", match.scriptName);
		addEnv(env, "PATH_INFO", match.pathInfo);
		addEnv(env, "QUERY_STRING", request.query());
		addEnv(env, "CONTENT_LENGTH", toString(request.body().size()));
		addEnv(env, "CONTENT_TYPE", contentType);
		addEnv(env, "SERVER_NAME", request.getHost());
		addEnv(env, "SERVER_PORT", toString(server.port));
		addEnv(env, "REMOTE_ADDR", remoteAddress);
		addEnv(env, "REDIRECT_STATUS", "200");

		for(std::map<std::string, std::string>::const_iterator it = headers.begin();
				it != headers.end(); ++it)
		{
			std::string name = toLower(it->first);

			if(name == "content-type" || name == "content-length")
			{
				continue;
			}
			addEnv(env, "HTTP_" + toUpperHeaderName(it->first), it->second);
		}
		return env;
	}

	void buildCharArray(const std::vector<std::string>& strings,
			std::vector<char*>& result)
	{
		result.clear();
		for(std::size_t i = 0; i < strings.size(); i++)
		{
			result.push_back(const_cast<char*>(strings[i].c_str()));
		}
		result.push_back(NULL);
	}

	int parseStatusValue(const std::string& value)
	{
		std::string trimmed = trim(value);

		if(trimmed.size() < 3)
		{
			return 0;
		}
		if(trimmed[0] < '0' || trimmed[0] > '9'
				|| trimmed[1] < '0' || trimmed[1] > '9'
				|| trimmed[2] < '0' || trimmed[2] > '9')
		{
			return 0;
		}
		return (trimmed[0] - '0') * 100
			+ (trimmed[1] - '0') * 10
			+ (trimmed[2] - '0');
	}
}

CgiExecutor::CgiExecutor()
{
	resetInactive();
}

CgiExecutor::CgiExecutor(const CgiExecutor& /*other*/)
{
	resetInactive();
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

void CgiExecutor::resetInactive()
{
	_state = NOT_STARTED;
	_inputFd = -1;
	_outputFd = -1;
	_pid = -1;
	_input.clear();
	_inputOffset = 0;
	_output.clear();
	_startedAt = 0;
	_errorStatus = 0;
	_childReaped = true;
}

bool CgiExecutor::start(const HttpRequest& request,
		const ServerConfig& server,
		const LocationConfig& /*location*/,
		const CgiMatch& match,
		const std::string& remoteAddress)
{
	int inputPipe[2] = {-1, -1};
	int outputPipe[2] = {-1, -1};

	cleanup();
	resetInactive();
	if(pipe(inputPipe) < 0)
	{
		return fail(500);
	}
	if(pipe(outputPipe) < 0)
	{
		closeIfOpen(inputPipe[0]);
		closeIfOpen(inputPipe[1]);
		return fail(500);
	}

	_pid = fork();
	if(_pid < 0)
	{
		closeIfOpen(inputPipe[0]);
		closeIfOpen(inputPipe[1]);
		closeIfOpen(outputPipe[0]);
		closeIfOpen(outputPipe[1]);
		return fail(500);
	}

	if(_pid == 0)
	{
		std::vector<std::string> argvStrings;
		std::vector<std::string> envStrings;
		std::vector<char*> argv;
		std::vector<char*> envp;

		if(dup2(inputPipe[0], STDIN_FILENO) < 0
				|| dup2(outputPipe[1], STDOUT_FILENO) < 0)
		{
			std::exit(1);
		}
		closeIfOpen(inputPipe[0]);
		closeIfOpen(inputPipe[1]);
		closeIfOpen(outputPipe[0]);
		closeIfOpen(outputPipe[1]);

		argvStrings.push_back(match.interpreter);
		argvStrings.push_back(match.scriptFilename);
		buildCharArray(argvStrings, argv);
		envStrings = buildEnvironment(request, server, match, remoteAddress);
		buildCharArray(envStrings, envp);
		if(chdir(directoryName(match.scriptFilename).c_str()) < 0)
		{
			std::exit(1);
		}
		execve(match.interpreter.c_str(), &argv[0], &envp[0]);
		std::exit(1);
	}

	closeIfOpen(inputPipe[0]);
	closeIfOpen(outputPipe[1]);
	if(!setNonBlocking(inputPipe[1]) || !setNonBlocking(outputPipe[0]))
	{
		closeIfOpen(inputPipe[1]);
		closeIfOpen(outputPipe[0]);
		killChild();
		return fail(500);
	}
	_inputFd = inputPipe[1];
	_outputFd = outputPipe[0];
	_input = request.body();
	_inputOffset = 0;
	_output.clear();
	_startedAt = std::time(NULL);
	_childReaped = false;
	_state = RUNNING;
	if(_input.empty())
	{
		closeInput();
	}
	return true;
}

CgiExecutor::State CgiExecutor::state() const
{
	return _state;
}

int CgiExecutor::inputFd() const
{
	return _inputFd;
}

int CgiExecutor::outputFd() const
{
	return _outputFd;
}

pid_t CgiExecutor::pid() const
{
	return _pid;
}

int CgiExecutor::errorStatus() const
{
	return _errorStatus;
}

bool CgiExecutor::wantsInputWrite() const
{
	return _state == RUNNING && _inputFd != -1;
}

bool CgiExecutor::wantsOutputRead() const
{
	return _state == RUNNING && _outputFd != -1;
}

bool CgiExecutor::writeInput()
{
	ssize_t n;

	if(_state != RUNNING || _inputFd == -1)
	{
		return true;
	}
	if(_inputOffset >= _input.size())
	{
		closeInput();
		return true;
	}
	n = write(_inputFd, _input.data() + _inputOffset,
			_input.size() - _inputOffset);
	if(n > 0)
	{
		_inputOffset += static_cast<std::size_t>(n);
		if(_inputOffset >= _input.size())
		{
			closeInput();
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

	if(_state != RUNNING || _outputFd == -1)
	{
		return true;
	}
	n = read(_outputFd, buffer, sizeof(buffer));
	if(n > 0)
	{
		_output.append(buffer, static_cast<std::size_t>(n));
		return true;
	}
	if(n == 0)
	{
		closeOutput();
		reapChild();
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
	closeInput();
	closeOutput();
	killChild();
	resetInactive();
}

std::string CgiExecutor::buildHttpResponse(bool keepAlive) const
{
	std::size_t delimiter = _output.find("\r\n\r\n");
	std::size_t delimiterLength = 4;
	std::string headerBlock;
	std::string body;
	std::istringstream lines;
	std::string line;
	std::string headers;
	int status = 200;
	bool hasLocation = false;

	if(delimiter == std::string::npos)
	{
		delimiter = _output.find("\n\n");
		delimiterLength = 2;
	}
		if(delimiter == std::string::npos)
		{
			return buildSimpleResponse(502, keepAlive);
		}
	headerBlock = _output.substr(0, delimiter);
	body = _output.substr(delimiter + delimiterLength);
	lines.str(headerBlock);
	while(std::getline(lines, line))
	{
		std::size_t colon;
		std::string name;
		std::string value;
		std::string lowerName;

		if(!line.empty() && line[line.size() - 1] == '\r')
		{
			line.erase(line.size() - 1);
		}
		if(line.empty())
		{
			continue;
		}
		colon = line.find(':');
			if(colon == std::string::npos || colon == 0)
			{
				return buildSimpleResponse(502, keepAlive);
			}
		name = line.substr(0, colon);
		value = trim(line.substr(colon + 1));
		lowerName = toLower(name);
		if(lowerName == "status")
		{
			int parsedStatus = parseStatusValue(value);
				if(parsedStatus < 100 || parsedStatus > 599)
				{
					return buildSimpleResponse(502, keepAlive);
				}
			status = parsedStatus;
			continue;
		}
		if(lowerName == "content-length" || lowerName == "connection")
		{
			continue;
		}
		if(lowerName == "location")
		{
			hasLocation = true;
		}
		headers += name + ": " + value + "\r\n";
	}
	if(hasLocation && status == 200)
	{
		status = 302;
	}

	std::ostringstream response;
	response << "HTTP/1.1 " << status << " " << reasonPhrase(status)
		<< "\r\n";
	response << "Connection: " << (keepAlive ? "keep-alive" : "close")
		<< "\r\n";
	response << headers;
	response << "Content-Length: " << body.size() << "\r\n";
	response << "\r\n";
	response << body;
	return response.str();
}

bool CgiExecutor::fail(int status)
{
	_state = FAILED;
	_errorStatus = status;
	closeInput();
	closeOutput();
	killChild();
	return false;
}

void CgiExecutor::closeInput()
{
	closeIfOpen(_inputFd);
}

void CgiExecutor::closeOutput()
{
	closeIfOpen(_outputFd);
}

void CgiExecutor::killChild()
{
	if(_pid > 0 && !_childReaped)
	{
		int status;

		kill(_pid, SIGKILL);
		if(waitpid(_pid, &status, 0) == _pid)
		{
			_childReaped = true;
		}
	}
}

void CgiExecutor::reapChild()
{
	int status;
	pid_t result;

	if(_pid <= 0 || _childReaped)
	{
		return;
	}
	result = waitpid(_pid, &status, WNOHANG);
	if(result == _pid)
	{
		_childReaped = true;
	}
}
