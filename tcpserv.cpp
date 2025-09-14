/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tcpserv.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: totommi <totommi@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/09/10 14:29:28 by totommi           #+#    #+#             */
/*   Updated: 2025/09/12 12:01:33 by totommi          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "tcpserv.hpp"

#include <unistd.h> 	//close
#include <fcntl.h>		//well2...
#include <arpa/inet.h>	//accept
#include <errno.h>		//EGAIN, errno

#include <stdexcept> 	//runtime_error

#include <iostream>

/* ========================================================================== */
/* ============================ CONSTR & DESTR ============================== */
/* ========================================================================== */

tcpserv::tcpserv(): _port(-1), _f(NULL), _b(NULL) {}

tcpserv::tcpserv(int __port, front_desk_agent __f): _port(__port), _f(__f), _b(NULL)
{
	if (this->setup(_port) == 0)
		this->launch(_f);
}

tcpserv::~tcpserv()
{
	for (size_t i = 1; i < _fds.size(); ++i)
		close(_fds[1].fd);
}

/* ========================================================================== */
/* =============================== METHODS ================================== */
/* ========================================================================== */

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
}

/* we are assuming the socket is already been closed? NHA */
void tcpserv::kill(int __idx, const std::string& req)
{
	if (__idx < 0) {throw std::out_of_range("tcpserv::kill: negative index");}
	if (__idx == 0) {throw std::out_of_range("tcpserv::kill: invalid index");}
	if (__idx >= static_cast<ssize_t>(_fds.size())) {throw std::out_of_range("tcpserv::kill: index out of range");}

	/* kill signal to the reception module */
	(*_f)(std::pair<int, std::string>(_fds[__idx].fd, req));

	/* closing the socket */
	close(_fds[__idx].fd);

	/* remove from vector */
	if (__idx == static_cast<ssize_t>(_fds.size() - 1)) {_fds.pop_back();}
	else {_fds.erase(_fds.begin() + __idx);}
}

/* @param fds vector of pollfd structs with fds[1].fd being the listening socket.
accept()s the TPC connection, sets the socket to O_NONBLOCK and stores the
pollfd struct in the vector passed. */
int	tcpserv::accept_and_store()
{	
	struct sockaddr_in 	addr;
	int					fd = 0;
	socklen_t			len = sizeof(addr);
	
	/* ACCEPTS THE CONNECTION */
	std::memset(&addr, 0, sizeof(addr));
	if ((fd = accept(_fds[1].fd, (struct sockaddr*)(&addr), &len)) < 0)
	{
		std::cerr << "Accept failure" << std::strerror(errno) << std::endl;
		return (errno != ECONNABORTED) ? 1: 0;	// cases in which the server can stil run
	}
	
	/* check for poll() limits: RLIMIT_NOFILE */
	if (_fds.size() >= SERVER_CAPACITY)
	{
		std::cout << "Warn: Cannot accept: Server is full" << std::endl;
		close(fd); return 0;
	}

	/* setting socket to non blocking */
	fcntl(fd, F_SETFL, O_NONBLOCK);	

	/* STORES THE FD */
	this->add(fd);
	return 0;
}

/* expected a non blocking socket, proveeds to recv() call
until no more data is found */
int	tcpserv::receive_all_data(int sockfd, std::string& data)
{
	char		buff[1024];
	int			ret;

	data.clear();
	do
	{
		/* keeps reading the non-blocking socket
		until error or end-of-transmission */
		std::memset(buff, 0, sizeof(buff));
		ret = recv(sockfd, buff, sizeof(buff) - 1, 0);
		if (ret > 0) {data += buff;}
	}
	while (ret > 0);
	
	/* ERROR HANDLING */
	if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK)	//EAGAIN esce se nn ci sono piu' cose da lgggere
	{
		std::cerr << "Recv failrue: " << std::strerror(errno) << std::endl;
		return -1;
	}
	if (ret == 0)	// client ha chiuso il write end della pipe
	{
		// std::cout << "Client forcefully disconnected" << std::endl;
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
		std::cerr << "Socket failure: " << std::strerror(errno) << std::endl;
		return -1;
	}

	/* FIND LOCAL IP */
	// std::string local_ip;
	// if (fake_gethostname(local_ip))
	// 	return -1;

	/* ADDR CREATION */
	struct sockaddr_in	serv_addr;
	std::memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;							// <- Ipv4?
	serv_addr.sin_addr.s_addr = INADDR_ANY/* inet_addr(local_ip.c_str()) */;
	serv_addr.sin_port = htons(__port);						// on this port

	/* SETUP SOCKET OPTIONS */
	setsockopt(listfd, SOL_SOCKET, SO_REUSEADDR, NULL, 0);	// reusable port
	fcntl(listfd, F_SETFL, O_NONBLOCK);						// non-blocking recv/send

	/* BIND */
	if (bind(listfd, (struct sockaddr*)(&serv_addr), sizeof(serv_addr)) < 0)
	{
		std::cerr << "Bind failure: " << std::strerror(errno) << std::endl;
		close(listfd);
		return -1;
	}

	/* LISTEN */
	if (listen(listfd, 5) < 0)
	{
		std::cerr << "Listen failure: " << std::strerror(errno) << std::endl;
		close(listfd);
		return -1;
	}

	/* STORE VARIABLES */
	_port = __port;
	this->add(STDIN_FILENO);	//just for ctrl+D
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
	if (_b != NULL && _b(NULL)) {std::cerr << "Build failure" << std::endl; return;}

	std::cout << "launching tcpserv on port " << _port << std::endl;
	for (;;) //hehe
	{
		int ret = poll(&_fds[0], _fds.size(), 10);
		if (ret == 0) {continue;}
		if (ret == -1) {std::cout << "Poll failre: " << std::strerror(errno) << std::endl; break;}
		if (_fds[0].revents & POLLIN) {char c; if (read(STDIN_FILENO, &c, 1) <= 0) break;}
		if (_fds[1].revents & POLLIN) {if (accept_and_store()) break;}
		for (size_t i = 2; i < _fds.size(); ++i)
		{
			std::string data;	/* recv buffer */
			if (_fds[i].revents & (POLLHUP | POLLERR)) {this->kill(i); --i; continue;}
			else if (_fds[i].revents & POLLIN)
			{
				if (receive_all_data(_fds[i].fd, data) < 0) {this->kill(i); --i; continue;}
				else if (data.find(GRACE_LEAVE) == 0)
				{
					/* client graceful disconnect */
					std::cout << "Client " << _fds[i].fd << " disconnected" << std::endl;
					this->kill(i, data); --i; continue;
				}
				/* sending data to the reception module */
				else {(*_f)(std::pair<int, std::string>(_fds[i].fd, data));}
			}
		}
	}
}

