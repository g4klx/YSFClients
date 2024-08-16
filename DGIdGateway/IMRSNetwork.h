/*
 *   Copyright (C) 2009-2014,2016,2017,2018,2020,2021,2024 by Jonathan Naylor G4KLX
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
#include "Timer.h"

#include <cstdint>
#include <vector>
#include <string>
#include <random>

class IMRSDest {
public:
	IMRSDest() :
	m_addr(),
	m_addrLen(0U),
	m_dgId(0U),
	m_state(DS_NOTLINKED),
	m_timer(1000U, 5U)
	{
	}

	sockaddr_storage m_addr;
	unsigned int     m_addrLen;
	unsigned int     m_dgId;
	DGID_STATUS      m_state;
	CTimer           m_timer;
};

class IMRSDGId {
public:
	IMRSDGId() :
	m_dgId(0U),
	m_name(),
	m_seqNo(0U),
	m_origin(NULL),
	m_source(NULL),
	m_dest(NULL),
	m_csd1(NULL),
	m_csd2(NULL),
	m_destinations(),
	m_debug(false),
	m_buffer(1000U, "IMRS Buffer")
	{
		m_origin = new unsigned char[YSF_CALLSIGN_LENGTH];
		m_source = new unsigned char[YSF_CALLSIGN_LENGTH];
		m_dest   = new unsigned char[YSF_CALLSIGN_LENGTH];
		m_csd1   = new unsigned char[2U * YSF_CALLSIGN_LENGTH];
		m_csd2   = new unsigned char[2U * YSF_CALLSIGN_LENGTH];
	}

	~IMRSDGId()
	{
		delete[] m_origin;
		delete[] m_source;
		delete[] m_dest;
		delete[] m_csd1;
		delete[] m_csd2;
	}

	unsigned int               m_dgId;
	std::string                m_name;
	uint16_t                   m_seqNo;
	unsigned char*             m_origin;
	unsigned char*             m_source;
	unsigned char*             m_dest;
	unsigned char*             m_csd1;
	unsigned char*             m_csd2;
	std::vector<IMRSDest*>     m_destinations;
	bool                       m_debug;
	CRingBuffer<unsigned char> m_buffer;
};

class CIMRSNetwork : public CDGIdNetwork {
public:
	CIMRSNetwork(const std::string& callsign);
	virtual ~CIMRSNetwork();

	void addDGId(unsigned int dgId, const std::string& name, const std::vector<IMRSDest*>& destinations, bool debug);

	virtual std::string getDesc(unsigned int dgId);

	virtual unsigned int getDGId();

	virtual bool open();

	virtual DGID_STATUS getStatus();

	virtual void link();

	virtual void write(unsigned int dgId, CYSFFICH& fich, const unsigned char* data);

	virtual unsigned int read(unsigned int dgId, unsigned char* data);

	virtual void clock(unsigned int ms);

	virtual void unlink();

	virtual void close();

private:
	CUDPSocket             m_socket;
	std::string            m_callsign;
	std::vector<IMRSDGId*> m_dgIds;
	std::mt19937           m_random;
	uint16_t               m_streamId;

	bool      find(const sockaddr_storage& addr, IMRSDGId*& ptr, IMRSDest*& dest) const;
	IMRSDGId* find(unsigned int dgId) const;

	bool writePongConnect(const IMRSDest& dest, bool debug);
	bool writeHeader(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data);
	bool writeData(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data);
	bool writeTerminator(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data);
	bool writePing(const IMRSDest& dest, bool debug);

	void readHeader(IMRSDGId* ptr, const unsigned char* data);
	void readData(IMRSDGId* ptr, const unsigned char* data);
	void readTerminator(IMRSDGId* ptr, const unsigned char* data);
};

#endif
