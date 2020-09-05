/*
 *   Copyright (C) 2009-2014,2016,2017,2018 by Jonathan Naylor G4KLX
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

#ifndef	FCSNetwork_H
#define	FCSNetwork_H

#include "YSFDefines.h"
#include "UDPSocket.h"
#include "RingBuffer.h"
#include "Timer.h"

#include <cstdint>
#include <string>
#include <map>

enum FCS_STATE {
	FCS_UNLINKED,
	FCS_LINKING,
	FCS_LINKED
};

class CFCSNetwork {
public:
	CFCSNetwork(unsigned int port, const std::string& callsign, unsigned int rxFrequency, unsigned int txFrequency, const std::string& locator, unsigned int id, bool debug);
	~CFCSNetwork();

	bool open();

	void setOptions(const std::string& options);

	void clearDestination();

	void write(const unsigned char* data);

	bool writeLink(const std::string& reflector);

	void writeUnlink(unsigned int count = 1U);

	unsigned int read(unsigned char* data);

	void clock(unsigned int ms);

	void close();

private:
	CUDPSocket                     m_socket;
	bool                           m_debug;
	in_addr                        m_address;
	unsigned char*                 m_ping;
	unsigned char*                 m_options;
	std::string                    m_opt;
	unsigned char*                 m_info;
	std::string                    m_reflector;
	std::string                    m_print;
	CRingBuffer<unsigned char>     m_buffer;
	std::map<std::string, in_addr> m_addresses;
	unsigned char                  m_n;
	CTimer                         m_pingTimer;
	CTimer                         m_resetTimer;
	FCS_STATE                      m_state;

	void writeOptions();
	void writeInfo();
	void writePing();
};

#endif
