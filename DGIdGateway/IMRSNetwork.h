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

#ifndef	IMRSNetwork_H
#define	IMRSNetwork_H

#include "DGIdNetwork.h"
#include "YSFDefines.h"
#include "UDPSocket.h"
#include "RingBuffer.h"
#include "YSFDefines.h"
#include "YSFFICH.h"

#include <cstdint>
#include <vector>
#include <string>

struct IMRSDest {
	sockaddr_storage m_addr;
	unsigned int     m_addrLen;
	unsigned int     m_dgId;
};

class IMRSDGId {
public:
	IMRSDGId() :
	m_dgId(0U),
	m_name(),
	m_seqNo(0U),
	m_source(nullptr),
	m_dest(nullptr),
	m_destinations(),
	m_debug(false),
	m_buffer(1000U, "IMRS Buffer")
	{
		m_source = new unsigned char[YSF_CALLSIGN_LENGTH];
		m_dest   = new unsigned char[YSF_CALLSIGN_LENGTH];
	}

	~IMRSDGId()
	{
		delete[] m_source;
		delete[] m_dest;
	}

	unsigned int               m_dgId;
	std::string                m_name;
	uint16_t                   m_seqNo;
	unsigned char*             m_source;
	unsigned char*             m_dest;
	std::vector<IMRSDest*>     m_destinations;
	bool                       m_debug;
	CRingBuffer<unsigned char> m_buffer;
};

class CIMRSNetwork : public CDGIdNetwork {
public:
	CIMRSNetwork();
	virtual ~CIMRSNetwork();

	void addDGId(unsigned int dgId, const std::string& name, const std::vector<IMRSDest*>& destinations, bool debug);

	virtual std::string getDesc(unsigned int dgId);

	virtual unsigned int getDGId();

	virtual bool open();

	virtual DGID_STATUS getStatus();

	virtual void link();

	virtual void write(unsigned int dgId, const unsigned char* data);

	virtual unsigned int read(unsigned int dgId, unsigned char* data);

	virtual void clock(unsigned int ms);

	virtual void unlink();

	virtual void close();

private:
	CUDPSocket             m_socket;
	std::vector<IMRSDGId*> m_dgIds;
	DGID_STATUS            m_state;

	IMRSDGId* find(const sockaddr_storage& address) const;
	IMRSDGId* find(unsigned int dgId) const;

	bool writeHeaderTrailer(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data);
	bool writeData(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data);

	void readHeaderTrailer(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data);
	void readData(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data);
};

#endif
