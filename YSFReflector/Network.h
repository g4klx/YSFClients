/*
 *   Copyright (C) 2009-2014,2016,2020 by Jonathan Naylor G4KLX
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

#ifndef	Network_H
#define	Network_H

#include "YSFDefines.h"
#include "UDPSocket.h"
#include "Timer.h"

#include <cstdint>
#include <string>

class CNetwork {
public:
	CNetwork(unsigned int port, unsigned int id, const std::string& name, const std::string& description, bool debug);
	~CNetwork();

	bool open(const std::string& bindaddr);

	bool writeData(const unsigned char* data, const in_addr& address, unsigned int port);
	bool writePoll(const in_addr& address, unsigned int port);

	unsigned int readData(unsigned char* data, unsigned int length, in_addr& address, unsigned int& port);

	void close();

	void setCount(unsigned int count);

private:
	CUDPSocket   m_socket;
    unsigned int m_id;
	std::string  m_name;
	std::string  m_description;
	std::string  m_callsign;
	bool         m_debug;
	unsigned char* m_status;
};

#endif
