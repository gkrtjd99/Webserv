#ifndef CGIPROCESS_HPP
# define CGIPROCESS_HPP

#include <string>
#include <sys/types.h>
#include <vector>

class CgiProcess
{
public:
	CgiProcess();
	CgiProcess(const CgiProcess& other);
	CgiProcess& operator=(const CgiProcess& other);
	~CgiProcess();

	bool start(const std::string& interpreter,
			const std::string& scriptFilename,
			const std::vector<std::string>& envStrings);
	void cleanup();
	void resetInactive();

	int inputFd() const;
	int outputFd() const;
	pid_t pid() const;
	bool inputOpen() const;
	bool outputOpen() const;

	void closeInput();
	void closeOutput();
	void killChild();
	void reapChild();

private:
	int _inputFd;
	int _outputFd;
	pid_t _pid;
	bool _childReaped;

	bool setNonBlocking(int fd) const;
	void closeIfOpen(int& fd) const;
	std::string directoryName(const std::string& path) const;
};

#endif
