/*
 *   Copyright (C) 2009-2014,2016,2017,2018,2020,2025 by Jonathan Naylor G4KLX
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

#ifndef	YSFNetwork_H
#define	YSFNetwork_H

#include "YSFDefines.h"
#include "YSFReflectors.h"
#include "UDPSocket.h"
#include "RingBuffer.h"
#include "Timer.h"

#include <cstdint>
#include <string>

class CYSFNetwork {
public:
	CYSFNetwork(const std::string& address, unsigned short port, const std::string& callsign, bool debug);
	CYSFNetwork(unsigned short port, const std::string& callsign, bool debug);
	~CYSFNetwork();

	bool setDestination(const std::string& name, const sockaddr_storage& addr, unsigned int addrLen);
	bool setDestination(const CYSFReflector& reflector);
	void clearDestination();

	void write(const unsigned char* data);

	void writePoll(unsigned int count = 1U);
	void setOptions(const std::string& options = "");
	void writeUnlink(unsigned int count = 1U);

	unsigned int read(unsigned char* data);

	void clock(unsigned int ms);

private:
	CUDPSocket                 m_socket;
	bool                       m_debug;
	CYSFReflector              m_reflector;
	unsigned char*             m_poll;
	unsigned char*             m_options;
	std::string                m_opt;
	unsigned char*             m_unlink;
	CRingBuffer<unsigned char> m_buffer;
	CTimer                     m_pollTimer;
	bool                       m_linked;
	bool                       m_ipV6;

	bool open();
	void close();
};

#endif
