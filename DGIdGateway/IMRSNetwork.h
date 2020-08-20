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

#ifndef	IMRSNetwork_H
#define	IMRSNetwork_H

#include "DGIdNetwork.h"
#include "YSFDefines.h"
#include "UDPSocket.h"
#include "RingBuffer.h"

#include <vector>
#include <string>

struct IMRSDest {
	in_addr      m_address;
	unsigned int m_dgId;
};

class IMRSDGId {
public:
	IMRSDGId() :
	m_dgId(0U),
	m_destinations(),
	m_debug(false),
	m_buffer(1000U, "IMRS Buffer")
	{}

	unsigned int               m_dgId;
	std::vector<IMRSDest*>     m_destinations;
	bool                       m_debug;
	CRingBuffer<unsigned char> m_buffer;
};

class CIMRSNetwork : public CDGIdNetwork {
public:
	CIMRSNetwork();
	virtual ~CIMRSNetwork();

	void addDGId(unsigned int dgId, const std::vector<IMRSDest*>& destinations, bool debug);

	virtual bool open();

	virtual void link();

	virtual void write(unsigned int dgId, const unsigned char* data);

	virtual unsigned int read(unsigned int dgId, unsigned char* data);

	virtual void clock(unsigned int ms);

	virtual void unlink();

	virtual void close();

private:
	CUDPSocket             m_socket;
	std::vector<IMRSDGId*> m_dgIds;

	IMRSDGId* find(in_addr address) const;
	IMRSDGId* find(unsigned int dgId) const;
};

#endif
