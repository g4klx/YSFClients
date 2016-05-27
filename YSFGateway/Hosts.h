/*
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef	Hosts_H
#define	Hosts_H

#include <string>
#include <vector>
#include <map>

class CYSFHost {
public:
	std::string  m_address;
	unsigned int m_port;
};

class CHosts {
public:
	CHosts(const std::string& filename);
	~CHosts();

	bool read();

	CYSFHost* find(const std::string& name) const;

	std::vector<std::string>& list() const;

private:
	std::string                      m_filename;
	std::map<std::string, CYSFHost*> m_table;
};

#endif
