/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: topiana- <topiana-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/09/18 13:34:42 by topiana-          #+#    #+#             */
/*   Updated: 2025/09/22 18:28:51 by topiana-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "tcpserv.hpp"

#include <iostream>

int	rebound(const std::pair<int, std::string>& req, std::string& res)
{
	(void)req;
	// res = "tcpserv: " + req.second;
	// std::cout << "res is '" << res << "'" << std::endl;
	// if (req.second.rfind("\r\n\r\n") == req.second.length() - 4);
		res = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 75\r\nConnection: Closed\r\n\r\n<html><h1>OMG SO COOOOLLL</h1><body>man pls stop loading...</body></html>\r\n\r\n";
	// res = "HTTP/1.1 200 OK\r\nDate: Mon, 27 Jul 2009 12:28:53 GMT\r\nServer: Apache/2.2.14 (Win32)\r\nLast-Modified: Wed, 22 Jul 2009 19:15:56 GMT\r\nContent-Length: 88\r\nContent-Type: text/html\r\nConnection: Closed\r\n\r\n<html><head>OMG SO COOOOLLL<head><html>\r\n\r\n";
	return -1;
}

// #include <signal.h>

// int	sendByte(int c, pid_t pid)
// {
// 	for (int i = 0; i < 8; ++i)
// 	{
// 		if ((c << i) & 1)
// 			kill(pid, SIGUSER1);
// 		else
// 			kill(pid, SIGUSER2);
// 	}
// }

// void	sendBitString(const std::string& str, pid_t pid)
// {
// 	for (size_t i = 0;< i < str.length(); ++i)
// 		sendByte(str.at(i), pid);
// }

#include <string.h>		//memset
#include <arpa/inet.h>
#include <sys/socket.h>	//socket, conect
#include <sys/types.h>
#include <unistd.h>		//close

// int					g_tcp_socket = -1;
std::string 		g_modules_list = "localhost:5000";

// int	addModule(void *ptr)
// {
// 	(void)ptr;

// 	int	port = std::atoi(&g_modules_list[g_modules_list.find(":") + 1]);
// 	std::cout << "port for service " << port << std::endl;

// 	/* ADDR CREATION */
// 	memset(&g_module_addr, 0, sizeof(g_module_addr));
// 	g_module_addr.sin_family = AF_INET;							// IPv4
// 	g_module_addr.sin_port = htons(port);						// Port number
// 	g_module_addr.sin_addr.s_addr = inet_addr("127.0.0.1");		// Loopback IP
// 	return 0;
// }

tcpserv *g_serv;

#include <algorithm>

int	forward(const std::pair<int, std::string>& req, std::string& res)
{
	static std::vector<int>				fwds;		/* forwarded requests */
	static std::vector<std::string *>	fwds_res;	/* forwarded responses */
	static std::vector<int>				heads;

	std::cout << __func__ << " message from fd " << req.first << std::endl;

	/* filter quit */
	if (req.second == "QUIT\r\n")
	{
		size_t idx = 0; while (idx < fwds.size() && fwds[idx] != req.first) ++idx;
		if (idx != fwds.size())
		{
			fwds.erase(fwds.begin() + idx);
			fwds_res.erase(fwds_res.begin() + idx);
		}
		std::vector<int>::iterator it = std::find(heads.begin(), heads.end(), req.first);
		if (it != heads.end()) {heads.erase(it);}
		return -1;
	}

	if (req.second.find("OPTIONS") != std::string::npos)
	{
		res = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\nContent-Length: 0\r\n\r\n";
		std::cout << "res set" << std::endl;
		return 0;
	}

	if (req.second.find("HTTP/") != std::string::npos)
	{
		heads.push_back(req.first);
		std::cout << "Added header" << std::endl;
		// std::cout << "returning " << &req.second[req.second.find("Content-Length: ")] << " from '" << req.second << std::endl;
		return std::atoi(&req.second[req.second.find("Content-Length: ") + 16]);
	}

	/* add forward request */
	if (std::find(heads.begin(), heads.end(), req.first) != heads.end())
	// if (std::find(fwds.begin(), fwds.end(), req.first) == fwds.end())
	{
		int	fd = g_serv->forward("localhost:5000", req.second);
		if (fd < 0) {res = "Error\r\n"; return 0;}
		else
		{
			std::cout << "forward fd " << fd << std::endl;
			fwds.push_back(fd);
			fwds_res.push_back(&res);
			heads.erase(std::find(heads.begin(), heads.end(), req.first));
		}
	}

	/* write forward responses */
	for (size_t i = 0; i < fwds.size(); ++i)
	{
		if (fwds[i] == req.first)
		{
			std::cout << "this is the response from the forwarded request " << fwds[i] << std::endl;
			(*fwds_res[i]) = req.second;
			if (i == fwds.size() - 1){fwds.pop_back(); fwds_res.pop_back();}
			else {fwds.erase(fwds.begin() + i); fwds_res.erase(fwds_res.begin() + i);}
			return -1;
		}
	}
	return 0;
}

#include <unistd.h> //close

int	main(int argc, char *argv[])
{
	tcpserv server;

	g_serv = &server;

	// if (argc != 3) {return 1;}
	if (argc >= 2) {g_modules_list = argv[1];}

	if (server.setup(4002) < 0) {return 1;}
	server.launch(&forward);
	return 0;
}
