/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   oaklog.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: topiana- <topiana-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/09/19 11:52:00 by topiana-          #+#    #+#             */
/*   Updated: 2025/09/19 19:49:46 by topiana-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "oaklog.hpp"
#include <iostream>

/* ========================================================================== */
/* ============================ CONSTR & DESTR ============================== */
/* ========================================================================== */

oaklog::oaklog(void): _logpath(DEFAULT_LOGFILE), _min_level(0), _console(true)
{
	_logfile.open(_logpath.c_str(), std::ios_base::app);
	if (!_logfile.is_open()) {throw CantOpenLogfileExcept();}
}

oaklog::~oaklog(void)
{
	_logfile.close();
}

oaklog::CantOpenLogfileExcept::CantOpenLogfileExcept() {}
oaklog::InvalidLogLevelExcept::InvalidLogLevelExcept() {}

/* ========================================================================== */
/* =============================== METHODS ================================== */
/* ========================================================================== */

const char* oaklog::CantOpenLogfileExcept::what() const throw() {return "Can't open logfile";}
const char* oaklog::InvalidLogLevelExcept::what() const throw() {return "Invalid log level";}


void	oaklog::chop(int __lvl, const std::string& __msg,
				const char* __file, int __line, const char* __func)
{
	/* Min level check */
	if (__lvl > _min_level) {return;}

	/* Max level cap */
	if (__lvl > _max_level) {__lvl = _max_level;}

	/* Write in the file */
	if (_logfile.is_open())
	{

		_logfile << "[" << _loglvl[__lvl] << "] "
				<< __file << ":" << __line << " (" << __func << ") - "
				<< __msg << std::endl;
	}
	/* Write in the console */
	if (_console == true)
	{
		if (__lvl == _max_level)
		{
			std::cerr << "[" << _loglvl[__lvl] << "] "
					<< __file << ":" << __line << " (" << __func << ") - "
					<< __msg << std::endl;
		}
		else
		{
			std::cout << "[" << _loglvl[__lvl] << "] "
					<< __file << ":" << __line << " (" << __func << ") - "
					<< __msg << std::endl;
		}
	}
}

#include <sstream>

void	oaklog::chop(int __lvl, const std::string& __msg, int __i,
				const char* __file, int __line, const char* __func)
{
	std::stringstream msg; msg << __msg << __i;
	chop(__lvl, msg.str(), __file, __line, __func);
}

void	oaklog::chop(int __lvl, const std::string& __msg, const std::string& __detail,
				const char* __file, int __line, const char* __func)
{
	std::stringstream msg; msg << __msg << __detail;
	chop(__lvl, msg.str(), __file, __line, __func);
}

void	oaklog::console(void) {_console = (_console == true) ? false : true;}

void	oaklog::setlvl(int __lvl)
{
	if (__lvl > _max_level || __lvl < 0) {throw InvalidLogLevelExcept();}
	_min_level = __lvl;
}
