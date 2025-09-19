/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: topiana- <topiana-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/09/18 13:34:42 by topiana-          #+#    #+#             */
/*   Updated: 2025/09/19 19:31:33 by topiana-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "tcpserv.hpp"

#include <iostream>

int	rebound(const std::pair<int, std::string>& req, std::string& res)
{
	// res = "tcpserv: " + req.second;
	// std::cout << "res is '" << res << "'" << std::endl;
	res = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 75\r\nConnection: Closed\r\n\r\n<html><h1>OMG SO COOOOLLL</h1><body>man pls stop loading...</body></html>\r\n\r\n";
	// res = "HTTP/1.1 200 OK\r\nDate: Mon, 27 Jul 2009 12:28:53 GMT\r\nServer: Apache/2.2.14 (Win32)\r\nLast-Modified: Wed, 22 Jul 2009 19:15:56 GMT\r\nContent-Length: 88\r\nContent-Type: text/html\r\nConnection: Closed\r\n\r\n<html><head>OMG SO COOOOLLL<head><html>\r\n\r\n";
	return 0;
}



int	main(void)
{
	tcpserv server(8080, &rebound);
	return 0;
}
