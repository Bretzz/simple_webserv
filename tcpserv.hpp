/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tcpserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: topiana- <topiana-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/09/10 14:20:03 by totommi           #+#    #+#             */
/*   Updated: 2025/09/22 16:11:16 by topiana-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TCPSERV_HPP
# define TCPSERV_HPP

# include <vector>
# include <deque>
# include <list>
# include <string>
// # include <pair>
# include <poll.h>

// #include <map>	//well3...

# include "oaklog.hpp"

# define SERVER_CAPACITY	64		/* Max number oof users */
# define TCPSERV_EOT		"\r\n"
# define GRACE_LEAVE 		"QUIT"	/* string the user sends befoe leaving */

/*  */
enum e_type
{
	USER,
	SERVICE
};

struct http_req // httpReq // http_req // reqhttp // 
{
	std::string head;		/* head of the request, works also as a buffer for non HTTP requests */
	std::string	body;		/* last request sent, with trailing data if present */
	size_t		bodylen;	/* expeted body length */
	std::string	response;	/* response processed */
	enum e_type	type;		/* service or user */
};

/* at construction this object will
bind(), setsock(), fcntl(), listen()
a socket at the port passed */
class tcpserv
{
	public:
		typedef int	(*front_desk_agent)(const std::pair<int, std::string>&, std::string&);
		typedef int	(*build)(void *);

	private:
		std::vector<struct pollfd>	_fds;		/* pollfd struct woth servfd at idx 0 */
		std::deque<struct http_req>	_reqs;		/* all to know about that request */
		int							_port;
		front_desk_agent			_f;			/* function that will receive all tcp reqests */
		build						_b;			/* function to be called when the tcp server just booted */
		oaklog						_log;		/* the best logger that ever lived */

		/* @param __idx the index of the client to kill in the _fds vector
		@throws out_of_range exeption if __idx is negaive or >= _fds's size */
		void	kill(int __idx, const std::string& req = "QUIT\r\n");
		/* @param __fd socket to be added to _fds vector
		@throws runtime_error if __fd is already present or a negative number */
		void	add(int __fd, enum e_type __type = USER, int __events = POLLIN | POLLOUT /* | POLLRDHUP */);
	
		/* @param fds vector of pollfd structs with fds[1].fd being the listening socket.
		accept()s the TPC connection, sets the socket to O_NONBLOCK and stores the
		pollfd struct in the vector passed.
		@returns 0: continue running, 1: fatal error. */
		int		accept_and_store(void);
		/* @param __idx index of the socket both in _fds and _requests.
	
		Performs one recv() call on the _fds[__idx] socket,
		then stores the buffer in _requests[__idx]
		@returns -1 if recv() failed to receive or the connection was closed, 0 otherwhise */
		int		receive_and_store(int, int __size = 1024);
		/* @param __idx index of the socket both in _fds and _requests.
	
		Crops _requests[__idx] until DELIMITER is found, then calls:
		
		front_desk_agent, with:
		
		- (_fds[__idx].fd, cropped_req) as pair
		
		- _responses[__idx] as buffer.
		@returns -1 if front_desk_agent signales to kill the users (returns -1).
		0 if the processing of the response went well*/
		int		crop_and_process(int);

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


		void	shutdown(void);
		// runtime change of port/front_desk_agent
		// void	setReader(reader);		#todo
		// void	setPort(int);			#todo

		/* @param __req request to forward to a local service
		@param __port port the service is listening on
		@returns -1 failure on socket initialization, sockfd if the request was forwarded*/
		int	forward(const std::string&, const std::string&);
};

#endif