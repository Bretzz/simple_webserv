/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tcpserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: topiana- <topiana-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/09/10 14:20:03 by totommi           #+#    #+#             */
/*   Updated: 2025/09/19 19:17:51 by topiana-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TCPSERV_HPP
# define TCPSERV_HPP

# include <vector>
# include <string>
// # include <pair>
# include <poll.h>

# include "oaklog.hpp"

# define SERVER_CAPACITY	64		/* Max number oof users */
# define DELIMITER			"\r\n\r\n"
# define GRACE_LEAVE 		"QUIT"	/* string the user sends befoe leaving */

/* at construction this object will
bind(), setsock(), fcntl(), listen()
a socket at the port passed */
class tcpserv
{
	public:
		typedef int	(*front_desk_agent)(const std::pair<int, std::string>&, std::string&);
		typedef int		(*build)(void *);

	private:
		std::vector<struct pollfd>	_fds;		/* pollfd struct woth servfd at idx 0 */
		std::vector<std::string>	_requests;	/* buffer wrote until DELIMITER */
		std::vector<std::string>	_responses;	/* resposnes processed by front_desk_agent */
		int							_port;
		front_desk_agent			_f;			/* function that will receive all tcp reqests */
		build						_b;			/* function to be called when the tcp server just booted */
		oaklog						_log;		/* the best logger that ever lived */

	public:
		tcpserv();
		tcpserv(int, front_desk_agent);
		~tcpserv();

		/* MAIN METHOFD */
		/* setup a non-blocking reusable socket to listen() on the port passed */
		int		setup(int);
		/* The poll() wrapper that accept()s new connection and forward the
		data received to the front_desk_agent */
		void	launch(front_desk_agent, build = NULL);

		/* @param __idx the index of the client to kill in the _fds vector
		@throws out_of_range exeption if __idx is negaive or >= _fds's size */
		void	kill(int __idx, const std::string& req = "QUIT\r\n");
		/* @param __fd socket to be added to _fds vector
		@throws runtime_error if __fd is already present or a negative number */
		void	add(int __fd, int __events = POLLIN | POLLOUT | POLLRDHUP);

		/* @param fds vector of pollfd structs with fds[1].fd being the listening socket.
		accept()s the TPC connection, sets the socket to O_NONBLOCK and stores the
		pollfd struct in the vector passed.
		@returns 0: continue running, 1: fatal error. */
		int		accept_and_store(void);
		/* @param __idx index of the socket both in _fds and _requests.
	
		Performs one recv() call on the _fds[__idx] socket,
		then stores the buffer in _requests[__idx]
		@returns -1 if recv() failed to receive or the connection was closed, 0 otherwhise */
		int		receive_and_store(int);
		/* @param __idx index of the socket both in _fds and _requests.
	
		Crops _requests[__idx] until DELIMITER is found, then calls:
		
		front_desk_agent, with:
		
		- (_fds[__idx].fd, cropped_req) as pair
		
		- _responses[__idx] as buffer.
		@returns -1 if front_desk_agent signales to kill the users (returns 1).
		0 if the processing of the response went well*/
		int		crop_and_process(int);

		void	shutdown(void);
		// runtime change of port/front_desk_agent
		// void	setReader(reader);		#todo
		// void	setPort(int);			#todo
};

#endif