/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: topiana- <topiana-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/09/18 13:34:42 by topiana-          #+#    #+#             */
/*   Updated: 2025/09/18 13:46:41 by topiana-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "tcpserv.hpp"

#include <iostream>

int	rebound(const std::pair<int, std::string>& req, std::string& res)
{
	res = "tcpserv: " + req.second;
	std::cout << "res is '" << res << "'" << std::endl;
	return 0;
}

int	main(void)
{
	tcpserv server(4002, &rebound);
	return 0;
}
