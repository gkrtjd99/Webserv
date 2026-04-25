*This project has been created as part of the 42 curriculum by hakseong.*

# Webserv

## Description
Webserv is a custom HTTP server written in C++ 98. The objective of this project is to implement a functional web server that can handle HTTP requests and serve static websites, as well as execute CGI scripts. 

Key features include:
- Non-blocking I/O operations managed by a single `poll()`, `kqueue()`, `epoll()`, or `select()`.
- Serving static files and directory listings.
- Support for HTTP GET, POST, and DELETE methods.
- File uploads from clients.
- Configuration via a configuration file (similar to NGINX) to set up ports, error pages, routes, and CGI executions.
- Execution of CGI scripts based on file extensions (e.g., `.php`, `.py`).

## Instructions
To compile the server, simply run:
```bash
make
```

Run the server by providing a configuration file:
```bash
./webserv [configuration_file]
```

You can then test the server using a standard web browser, `curl`, or `telnet`.

## Resources
- [HTTP/1.1 RFC 7230-7235](https://datatracker.ietf.org/doc/html/rfc7230)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [NGINX Documentation](https://nginx.org/en/docs/)
