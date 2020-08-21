/*
 *   Copyright (C) 2009-2014,2016,2017,2018,2020 by Jonathan Naylor G4KLX
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

#include "DGIdNetwork.h"
#include "YSFDefines.h"
#include "UDPSocket.h"
#include "RingBuffer.h"
#include "Timer.h"

#include <cstdint>
#include <string>

class CYSFNetwork : public CDGIdNetwork {
public:
	CYSFNetwork(const std::string& localAddress, unsigned int localPort, const std::string& name, const in_addr& address, unsigned int port, const std::string& callsign, bool debug);
	CYSFNetwork(unsigned int localPort, const std::string& name, const in_addr& address, unsigned int port, const std::string& callsign, bool debug);
	virtual ~CYSFNetwork();

	virtual std::string getDesc();

	virtual bool open();

	virtual void link();

	virtual void write(unsigned int dgId, const unsigned char* data);

	virtual unsigned int read(unsigned int dgId, unsigned char* data);

	virtual void clock(unsigned int ms);

	virtual void unlink();

	virtual void close();

private:
	CUDPSocket                 m_socket;
	bool                       m_debug;
	in_addr                    m_address;
	unsigned int               m_port;
	unsigned char*             m_poll;
	unsigned char*             m_unlink;
	CRingBuffer<unsigned char> m_buffer;
	CTimer                     m_pollTimer;
	std::string                m_name;
	bool                       m_linked;

	void writePoll();
};

#endif
