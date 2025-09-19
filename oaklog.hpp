/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   oaklog.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: topiana- <topiana-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/09/19 11:49:48 by topiana-          #+#    #+#             */
/*   Updated: 2025/09/19 17:49:18 by topiana-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef OAKLOH_HPP
# define OAKLOH_HPP

# include <string>
# include <fstream>

# define DEFAULT_LOGFILE "oaklog.log"

# define FLF __FILE__, __LINE__, __func__	/* File, Line, func where the FLF is written */

class oaklog
{
	public:
		enum lvl {Info, Warn, Error};
		
	private:
		const char			*_loglvl[4] = {"Info", "Warn", "Error"};
		static const short	_max_level = 2;
		
		/* actual attributes */
		std::string		_logpath;
		std::ofstream	_logfile;
		short			_min_level;
		bool			_console;

	public:
		oaklog();
		~oaklog();
	
		
	/* Enable/Disable console logging */
	void	console(void);

	/* Log literal strings, string + int, string + string */
	void	chop(int, const std::string&, const char*, int, const char*);
	void	chop(int, const std::string&, int, const char*, int, const char*);
	void	chop(int, const std::string&, const std::string&, const char*, int, const char*);
	
	/* EXCEPTIONS */
	class CantOpenLogfileExcept : public std::exception {
	public:
		CantOpenLogfileExcept();
		const char*	what() const throw();
	};
};

#endif