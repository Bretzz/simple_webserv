/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tcpserv.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: totommi <totommi@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/09/10 14:29:28 by totommi           #+#    #+#             */
/*   Updated: 2025/09/22 04:14:30 by totommi          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "tcpserv.hpp"

#include <unistd.h> 	//close
#include <fcntl.h>		//well2...
#include <arpa/inet.h>	//accept

#include <errno.h>		//EGAIN, errno
#include <cstring>		//strerror

#include <stdexcept> 	//runtime_error

#include <signal.h>		// signal

#include <map>

#include <iostream>

int	g_last_signal = 0;

/* -------------------------------------- */

static void	tcpserv_memset(void *ptr, int c, size_t n)
{
	unsigned char *c_ptr = (unsigned char *)ptr;

	for (size_t i = 0; i < n; ++i)
	{
		*c_ptr = c;
		++c_ptr;
	}
}

static void printBits(int byte)
{
	int bit = 0;
	while (bit <= 8)
	{
		if ((byte >> bit) & 1)
			std::cout << "1";
		else
			std::cout << "0";
		++bit;
	}
}

void	printStringBits(const std::string& str)
{
	for (size_t i = 0; i < str.length(); ++i) {
		printBits(str[i]);
		std::cout << " = '";
		if (std::isprint(str[i]))
			std::cout << str[i]; 
		else
			std::cout << static_cast<int>(str[i]);
		std::cout << "'" << std::endl;
	}
}

/* -------------------------------------- */

/* ========================================================================== */
/* ============================ CONSTR & DESTR ============================== */
/* ========================================================================== */

tcpserv::tcpserv(): _port(-1), _f(NULL), _b(NULL), _log() {}

tcpserv::tcpserv(int __port, front_desk_agent __f): _port(__port), _f(__f), _b(NULL), _log()
{
	if (this->setup(_port) == 0)
		this->launch(_f);
}

tcpserv::~tcpserv()
{
	for (size_t i = 0; i < _fds.size(); ++i)
		close(_fds[i].fd);
}

/* ========================================================================== */
/* =============================== METHODS ================================== */
/* ========================================================================== */

static void	setsig(int signal) {g_last_signal = signal;}

void	tcpserv::shutdown(void)
{
	if (g_last_signal == SIGINT)
	{
		_log.chop(0, "tcpserv shutting down ...", WHERE);

		/* Close all sockets */
		for (size_t i = 0; i < _fds.size(); ++i)
			close(_fds[i].fd);
	}
}

void	tcpserv::add(int __fd, int __events)
{
	/* negative check */
	if (__fd < 0) {throw std::runtime_error("tcpserv::add: negaitve fd");}
	/* duplicate check */
	for (size_t i = 0; i < _fds.size(); ++i) {if (_fds[i].fd == __fd) throw std::runtime_error("tcpserv::add: fd already exists");}

	/* logging ... */
	_log.chop(oaklog::lvl::Info, "adding: ", __fd, WHERE);

	struct pollfd	chicken;

	chicken.fd = __fd;
	chicken.events = __events;
	chicken.revents = 0;
	_fds.push_back(chicken);	// malloc failure?
	_requests.push_back("");
	_responses.push_back("");
}

/* we are assuming the socket is already been closed? NHA */
void tcpserv::kill(int __idx, const std::string& req)
{
	if (__idx < 0) {throw std::out_of_range("tcpserv::kill: negative index");}
	if (__idx == 0) {throw std::out_of_range("tcpserv::kill: invalid index");}
	if (__idx >= static_cast<ssize_t>(_fds.size())) {throw std::out_of_range("tcpserv::kill: index out of range");}

	/* logging ... */
	_log.chop(oaklog::lvl::Info, "killing: ", _fds[__idx].fd, WHERE);

	/* kill signal to the reception module */
	if (!req.empty()) {(*_f)(std::pair<int, std::string>(_fds[__idx].fd, req), _responses[__idx]);}

	/* send reception module response */
	if (!_responses[__idx].empty())
	{
		_log.chop(oaklog::lvl::Info, "kill response: ", _responses[__idx], WHERE);
		if (send(_fds[__idx].fd, _responses[__idx].c_str(), _responses[__idx].length(), 0) < 0) {_log.chop(oaklog::lvl::Error, "Send failure: ", std::strerror(errno), WHERE);}
	}

	/* closing the socket */
	close(_fds[__idx].fd);

	/* remove from fds vector */
	if (__idx == static_cast<ssize_t>(_fds.size() - 1)) {_fds.pop_back();}
	else {_fds.erase(_fds.begin() + __idx);}

	/* remove from requests vector */
	if (__idx == static_cast<ssize_t>(_requests.size() - 1)) {_requests.pop_back();}
	else {_requests.erase(_requests.begin() + __idx);}

	/* remove from responses vector */
	if (__idx == static_cast<ssize_t>(_responses.size() - 1)) {_responses.pop_back();}
	else {_responses.erase(_responses.begin() + __idx);}
}

/* @param fds vector of pollfd structs with fds[1].fd being the listening socket.
accept()s the TPC connection, sets the socket to O_NONBLOCK and stores the
pollfd struct in the vector passed. */
int	tcpserv::accept_and_store(void)
{	
	struct sockaddr_in 	addr;
	int					fd = 0;
	socklen_t			len = sizeof(addr);
	
	/* ACCEPTS THE CONNECTION */
	tcpserv_memset(&addr, 0, sizeof(addr));
	if ((fd = accept(_fds[0].fd, (struct sockaddr*)(&addr), &len)) < 0)
	{
		_log.chop(2, "Accept failure: ", std::strerror(errno), WHERE);
		return (errno != ECONNABORTED) ? 1: 0;	// cases in which the server can stil run
	}
	
	/* check for poll() limits: RLIMIT_NOFILE */
	if (_fds.size() >= SERVER_CAPACITY)
	{
		_log.chop(1, "Cannot accept: Server is full", WHERE);
		send(fd, "Server is full", 15, 0);
		close(fd); return 0;
	}

	/* setting socket to non blocking */
	fcntl(fd, F_SETFL, O_NONBLOCK);	

	/* STORES THE FD */
	this->add(fd);
	return 0;
}

/* after the pollfd struct ad "idx" has POLLIN in .revents,
a single recv() call is performed and the output stored in the _buff[idx] string */
int	tcpserv::receive_and_store(int __idx, int __size)
{
	char		buff[__size];
	int			ret;

	// std::cout << "recieveing index " << __idx << std::endl;

	{
		tcpserv_memset(buff, 0, sizeof(buff));
		ret = recv(_fds[__idx].fd, buff, sizeof(buff) - 1, 0);
		if (ret > 0) {_requests[__idx].append(buff, ret);}
	}
	
	// std::cout << "received buff '" << buff << "'" << std::endl;
	// std::cout << "received _req '" << _requests[__idx] << "'" << std::endl;
	
	/* ERROR HANDLING */
	if (ret < 0)
	{
		_log.chop(2, "Recv failure: ", std::strerror(errno), WHERE);
		return -1;
	}
	if (ret == 0)	// client ha chiuso il write end della pipe
	{
		_log.chop(1, "Client forcefully disconnected", WHERE);
		return -1;
	}

	return 0;
}

/* if a TCPSERV_EOT is found: crops the request found at _request[idx],
then passes the cropped request to front_desk_agent and stores the remaining
part. */
int	tcpserv::crop_and_process(int __idx)
{
	const size_t					term = _requests[__idx].rfind(TCPSERV_EOT, -1, (sizeof(TCPSERV_EOT) - 1));
	static std::map<int, size_t>	bodylen;		/* next recv of that size */

	if (bodylen.count(__idx) == 0 && term != std::string::npos)
	{
		/* in case of trailing data */
		std::string crop = _requests[__idx].substr(0, term + (sizeof(TCPSERV_EOT) - 1));
		_requests[__idx].erase(0, term + (sizeof(TCPSERV_EOT) - 1));
		
		_log.chop(0, "request: ", crop, WHERE);

		/* sendig buffer to process, if front_desk_agent returns 1, kill the bitch */
		int ret = 0;
		if ((ret = (*_f)(std::pair<int, std::string>(_fds[__idx].fd, crop), _responses[__idx])) < 0)
			return -1;
		if (ret > 0) {bodylen[__idx] = ret;}
	}
	else if (bodylen.count(__idx) > 0 && _requests[__idx].length() == bodylen[__idx])
	{
		_log.chop(0, "raw request: ", _requests[__idx], WHERE);

		bodylen.erase(__idx);

		/* sendig buffer to process, if front_desk_agent returns 1, kill the bitch */
		if ((*_f)(std::pair<int, std::string>(_fds[__idx].fd, _requests[__idx]), _responses[__idx]) < 0)
			return -1;
	}
	return 0;
}

/* @param __req request to forward to a local service
@param __port port the service is listening on
@returns -1 failure on socket initialization, sockfd if the request was forwarded*/
int	tcpserv::local_forward(int __port, const std::string& __req)
{
	/* ADDR CREATION */
	struct sockaddr_in	module_addr;
	memset(&module_addr, 0, sizeof(module_addr));
	module_addr.sin_family = AF_INET;						// IPv4
	module_addr.sin_port = htons(__port);					// Port number
	module_addr.sin_addr.s_addr = inet_addr("127.0.0.1");	// Loopback IP

	/* SOCKET CREATION */
	int	sockfd = -1;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		// std::cerr << "Socker failure: " << strerror(errno) << std::endl;
		_log.chop(oaklog::lvl::Error, "Socker failure: ", std::strerror(errno), WHERE);
		return -1;
	}

	/* connect to service */
	if (connect(sockfd, (struct sockaddr *)&module_addr, sizeof(module_addr)) < 0)
	{
		_log.chop(oaklog::lvl::Error, "Connect failure: ", std::strerror(errno), WHERE);
		return -1;
	}

	/* setting socket to non blocking */
	fcntl(sockfd, F_SETFL, O_NONBLOCK);

	/* send to service */
    if (send(sockfd, __req.c_str(), __req.length(), 0) < 0)
	{
		_log.chop(oaklog::lvl::Error, "Send failure: ", std::strerror(errno), WHERE);
		return -1;
	}

	// Signal no more data to send
	::shutdown(sockfd, SHUT_WR);

	std::cout << "forward server socket " << sockfd << std::endl;

	this->add(sockfd);
	return sockfd;
}

/* ------------------------------ MAIN METHONDS ---------------------------- */

/* = = = = = = = = = = = = = = = =  S E T U P = = = = = = = = = = = = = = = = */

int	tcpserv::setup(int __port)
{
	/* SOCKET */
	int	listfd = 0;
	if ((listfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		_log.chop(2, "Socket failure: ", std::strerror(errno), WHERE);
		return -1;
	}

	/* FIND LOCAL IP */
	// std::string local_ip;
	// if (fake_gethostname(local_ip))
	// 	return -1;

	/* ADDR CREATION */
	struct sockaddr_in	serv_addr;
	tcpserv_memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;							// <- Ipv4?
	serv_addr.sin_addr.s_addr = INADDR_ANY/* inet_addr(local_ip.c_str()) */;
	serv_addr.sin_port = htons(__port);						// on this port

	/* SETUP SOCKET OPTIONS */
	const int enable = 1;
	setsockopt(listfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));	// reusable port
	fcntl(listfd, F_SETFL, O_NONBLOCK);									// non-blocking recv/send

	/* BIND */
	if (bind(listfd, (struct sockaddr*)(&serv_addr), sizeof(serv_addr)) < 0)
	{
		_log.chop(2, "Bind failure: ", std::strerror(errno), WHERE);
		close(listfd);
		return -1;
	}

	/* LISTEN */
	if (listen(listfd, 5) < 0)
	{
		_log.chop(2, "Listen failure: ", std::strerror(errno), WHERE);
		close(listfd);
		return -1;
	}

	/* STORE VARIABLES */
	_port = __port;
	this->add(listfd);
	return 0;
}

/* = = = = = = = = = = = = = = = = L A U N C H = = = = = = = = = = = = = = = = */

void	tcpserv::launch(front_desk_agent __f, build __b)
{
	/* assign the functions */
	if (__f != NULL) {_f = __f;}
	else if (_f == NULL) {throw std::runtime_error("Error: Missing 'front_desk_agent' function");}

	/* build function */
	if (__b != NULL) {_b = __b;}
	if (_b != NULL && _b(NULL)) {_log.chop(oaklog::lvl::Error, "Build failure", WHERE); return;}

	/* catch SIGINT to quit gracefully */
	signal(SIGINT, &setsig);

	_log.chop(oaklog::lvl::Info, "launching tcpserv on port ", _port, WHERE);
	_log.console();

	for (;;) //hehe
	{
		int ret = poll(&_fds[0], _fds.size(), 10);
		if (g_last_signal == SIGINT) {this->shutdown(); break;}
		if (ret == 0) {/* health check routine */continue;}
		if (ret == -1) {_log.chop(oaklog::lvl::Error, "Poll failure: ", std::strerror(errno), WHERE); break;}
		if (_fds[0].revents & POLLIN) {if (accept_and_store()) break;}
		for (size_t idx = 1; idx < _fds.size(); ++idx)
		{
			/* Error cases */
			if (_fds[idx].revents & (/* POLLHUP | POLLRDHUP | */ POLLERR | POLLNVAL)) {this->kill(idx); --idx; continue;}
			else if (_fds[idx].revents & POLLIN)
			{
				/* single recv() call stored in _requests[idx] */
				if (receive_and_store(idx) < 0) {this->kill(idx); --idx; continue;}
				/* crops the request and sends it to the reception module */
				if (crop_and_process(idx) < 0) {this->kill(idx, ""); --idx; continue;}
			}
			else if (_fds[idx].revents & POLLOUT)
			{
				/* send the response */
				if (_responses[idx].empty()) {continue;}
				else if (send(_fds[idx].fd, _responses[idx].c_str(), _responses[idx].length(), 0) < 0) {_log.chop(oaklog::lvl::Error, "Send failure: ", std::strerror(errno), WHERE);}
				_log.chop(oaklog::lvl::Info, "resonpose: ", _responses[idx], WHERE);
				/* clear the responses */
				_responses[idx].clear();
			}
		}
	}
}

