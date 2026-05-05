#include "CgiProcess.hpp"
#include "CgiEnvironment.hpp"

#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

CgiProcess::CgiProcess()
{
	resetInactive();
}

CgiProcess::CgiProcess(const CgiProcess& /*other*/)
{
	resetInactive();
}

CgiProcess& CgiProcess::operator=(const CgiProcess& other)
{
	if(this != &other)
	{
		cleanup();
		resetInactive();
	}
	return *this;
}

CgiProcess::~CgiProcess()
{
	cleanup();
}

bool CgiProcess::start(const std::string& interpreter,
		const std::string& scriptFilename,
		const std::vector<std::string>& envStrings)
{
	int inputPipe[2] = {-1, -1};
	int outputPipe[2] = {-1, -1};

	cleanup();
	resetInactive();
	if(pipe(inputPipe) < 0)
	{
		return false;
	}
	if(pipe(outputPipe) < 0)
	{
		closeIfOpen(inputPipe[0]);
		closeIfOpen(inputPipe[1]);
		return false;
	}

	_pid = fork();
	if(_pid < 0)
	{
		closeIfOpen(inputPipe[0]);
		closeIfOpen(inputPipe[1]);
		closeIfOpen(outputPipe[0]);
		closeIfOpen(outputPipe[1]);
		return false;
	}

	if(_pid == 0)
	{
		CgiEnvironment environment;
		std::vector<std::string> argvStrings;
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

		argvStrings.push_back(interpreter);
		argvStrings.push_back(scriptFilename);
		environment.buildCharArray(argvStrings, argv);
		environment.buildCharArray(envStrings, envp);
		if(chdir(directoryName(scriptFilename).c_str()) < 0)
		{
			std::exit(1);
		}
		execve(interpreter.c_str(), &argv[0], &envp[0]);
		std::exit(1);
	}

	closeIfOpen(inputPipe[0]);
	closeIfOpen(outputPipe[1]);
	if(!setNonBlocking(inputPipe[1]) || !setNonBlocking(outputPipe[0]))
	{
		closeIfOpen(inputPipe[1]);
		closeIfOpen(outputPipe[0]);
		killChild();
		return false;
	}
	_inputFd = inputPipe[1];
	_outputFd = outputPipe[0];
	_childReaped = false;
	return true;
}

void CgiProcess::cleanup()
{
	closeInput();
	closeOutput();
	killChild();
	resetInactive();
}

void CgiProcess::resetInactive()
{
	_inputFd = -1;
	_outputFd = -1;
	_pid = -1;
	_childReaped = true;
}

int CgiProcess::inputFd() const
{
	return _inputFd;
}

int CgiProcess::outputFd() const
{
	return _outputFd;
}

pid_t CgiProcess::pid() const
{
	return _pid;
}

bool CgiProcess::inputOpen() const
{
	return _inputFd != -1;
}

bool CgiProcess::outputOpen() const
{
	return _outputFd != -1;
}

void CgiProcess::closeInput()
{
	closeIfOpen(_inputFd);
}

void CgiProcess::closeOutput()
{
	closeIfOpen(_outputFd);
}

void CgiProcess::killChild()
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

void CgiProcess::reapChild()
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

bool CgiProcess::setNonBlocking(int fd) const
{
	int flags = fcntl(fd, F_GETFL, 0);

	if(flags < 0)
	{
		return false;
	}
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

void CgiProcess::closeIfOpen(int& fd) const
{
	if(fd != -1)
	{
		close(fd);
		fd = -1;
	}
}

std::string CgiProcess::directoryName(const std::string& path) const
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
