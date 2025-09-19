/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tcpserv.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: topiana- <topiana-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/09/10 14:29:28 by totommi           #+#    #+#             */
/*   Updated: 2025/09/19 17:50:33 by topiana-         ###   ########.fr       */
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
		_log.chop(0, "tcpserv shutting down ...", FLF);

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

	/* kill signal to the reception module */
	if (!req.empty()) {(*_f)(std::pair<int, std::string>(_fds[__idx].fd, req), _responses[__idx]);}

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
		_log.chop(2, "Accept failure: ", std::strerror(errno), FLF);
		return (errno != ECONNABORTED) ? 1: 0;	// cases in which the server can stil run
	}
	
	/* check for poll() limits: RLIMIT_NOFILE */
	if (_fds.size() >= SERVER_CAPACITY)
	{
		_log.chop(1, "Cannot accept: Server is full", FLF);
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
int	tcpserv::receive_and_store(int __idx)
{
	char		buff[1024];
	int			ret;

	// std::cout << "recieveing index " << __idx << std::endl;

	tcpserv_memset(buff, 0, sizeof(buff));
	ret = recv(_fds[__idx].fd, buff, sizeof(buff) - 1, 0);
	if (ret > 0) {_requests[__idx] += buff;}
	
	// std::cout << "received buff '" << buff << "'" << std::endl;
	// std::cout << "received _req '" << _requests[__idx] << "'" << std::endl;
	
	/* ERROR HANDLING */
	if (ret < 0)
	{
		_log.chop(2, "Recv failure: ", std::strerror(errno), FLF);
		return -1;
	}
	if (ret == 0)	// client ha chiuso il write end della pipe
	{
		_log.chop(1, "Client forcefully disconnected", FLF);
		return -1;
	}

	return 0;
}

/* if a DELIMITER is found: crops the request found at _request[idx],
then passes the cropped request to front_desk_agent and stores the remaining
part. */
int	tcpserv::crop_and_process(int __idx)
{
	const size_t term = _requests[__idx].find(DELIMITER);

	if (term  != std::string::npos)
	{
		/* in case of trailing data */
		std::string single = _requests[__idx].substr(0, term + sizeof(DELIMITER) - 1);
		_requests[__idx].erase(0, term + sizeof(DELIMITER) - 1);

		_log.chop(0, "request: ", single, FLF);
		
		/* sendig buffer to process, if front_desk_agent returns 1, kill the bitch */
		if ((*_f)(std::pair<int, std::string>(_fds[__idx].fd, single), _responses[__idx]) == 1)
			return -1;
	}
	return 0;
}

/* ------------------------------ MAIN METHONDS ---------------------------- */

/* = = = = = = = = = = = = = = = =  S E T U P = = = = = = = = = = = = = = = = */

int	tcpserv::setup(int __port)
{
	/* SOCKET */
	int	listfd = 0;
	if ((listfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		_log.chop(2, "Socket failure: ", std::strerror(errno), FLF);
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
		_log.chop(2, "Bind failure: ", std::strerror(errno), FLF);
		close(listfd);
		return -1;
	}

	/* LISTEN */
	if (listen(listfd, 5) < 0)
	{
		_log.chop(2, "Listen failure: ", std::strerror(errno), FLF);
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
	if (_b != NULL && _b(NULL)) {_log.chop(oaklog::lvl::Error, "Build failure", FLF); return;}

	/* catch SIGINT to quit gracefully */
	signal(SIGINT, &setsig);

	_log.chop(oaklog::lvl::Info, "launching tcpserv on port ", _port, FLF);
	_log.console();

	for (;;) //hehe
	{
		int ret = poll(&_fds[0], _fds.size(), 10);
		if (g_last_signal == SIGINT) {this->shutdown(); break;}
		if (ret == 0) {/* health check routine */continue;}
		if (ret == -1) {_log.chop(oaklog::lvl::Error, "Poll failure: ", std::strerror(errno), FLF); break;}
		if (_fds[0].revents & POLLIN) {if (accept_and_store()) break;}
		for (size_t idx = 1; idx < _fds.size(); ++idx)
		{
			/* Error cases */
			if (_fds[idx].revents & (POLLHUP | POLLRDHUP | POLLERR)) {this->kill(idx); --idx; continue;}
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
				else if (send(_fds[idx].fd, _responses[idx].c_str(), _responses[idx].length(), 0) < 0) {_log.chop(oaklog::lvl::Error, "Send failure: ", std::strerror(errno), FLF);}
				_log.chop(oaklog::lvl::Info, "resonpose: ", _responses[idx], FLF);
				/* clear the responses */
				tcpserv_memset(&_fds[idx].revents, 0, sizeof(short));
				_responses[idx].clear();
			}
		}
	}
}

