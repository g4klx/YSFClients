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

#include "IMRSNetwork.h"
#include "YSFDefines.h"
#include "YSFFICH.h"
#include "Utils.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>

const unsigned int IMRS_PORT = 21110U;


CIMRSNetwork::CIMRSNetwork() :
m_socket(IMRS_PORT),
m_dgIds()
{
}

CIMRSNetwork::~CIMRSNetwork()
{
}

void CIMRSNetwork::addDGId(unsigned int dgId, const std::vector<IMRSDest*>& destinations, bool debug)
{
	IMRSDGId* f = new IMRSDGId;
	f->m_dgId         = dgId;
	f->m_destinations = destinations;
	f->m_debug        = debug;

	m_dgIds.push_back(f);
}

std::string CIMRSNetwork::getDesc()
{
	return "IMRS";
}

bool CIMRSNetwork::open()
{
	LogMessage("Opening IMRS network connection");

	return m_socket.open();
}

void CIMRSNetwork::write(unsigned int dgId, const unsigned char* data)
{
	assert(data != NULL);

	return;	// XXX Disable IMRS transmit

	IMRSDGId* ptr = find(dgId);
	if (ptr == NULL)
		return;

	CYSFFICH fich;
	fich.decode(data + 35U);

	unsigned char buffer[200U];

	// Create the header
	switch (fich.getDT()) {
	case YSF_DT_VD_MODE1:
		buffer[0U] = '0';
		buffer[1U] = '0';
		buffer[2U] = '0';
		// Copy the audio
		::memcpy(buffer + 35U + 0U,  data + 35U + 9U,  9U);
		::memcpy(buffer + 35U + 9U,  data + 35U + 27U, 9U);
		::memcpy(buffer + 35U + 18U, data + 35U + 45U, 9U);
		::memcpy(buffer + 35U + 27U, data + 35U + 63U, 9U);
		::memcpy(buffer + 35U + 36U, data + 35U + 81U, 9U);
		break;
	case YSF_DT_DATA_FR_MODE:
		buffer[0U] = '1';
		buffer[1U] = '1';
		buffer[2U] = '1';
		// Copy the audio
		::memcpy(buffer + 35U, data + 45U, 130U);
		break;
	case YSF_DT_VD_MODE2:
		buffer[0U] = '2';
		buffer[1U] = '2';
		buffer[2U] = '2';
		// Copy the audio
		::memcpy(buffer + 35U, data + 45U, 130U);
		break;
	case YSF_DT_VOICE_FR_MODE:
		buffer[0U] = '3';
		buffer[1U] = '3';
		buffer[2U] = '3';
		// Copy the audio
		::memcpy(buffer + 35U + 0U,  data + 35U + 9U,  18U);
		::memcpy(buffer + 35U + 18U, data + 35U + 27U, 18U);
		::memcpy(buffer + 35U + 36U, data + 35U + 45U, 18U);
		::memcpy(buffer + 35U + 54U, data + 35U + 63U, 18U);
		::memcpy(buffer + 35U + 72U, data + 35U + 81U, 18U);
		break;
	default:
		return;
	}

	// Copy the raw DCH

	for (std::vector<IMRSDest*>::const_iterator it = ptr->m_destinations.begin(); it != ptr->m_destinations.end(); ++it) {
		// Set the correct DG-ID for this destination
		fich.setDGId((*it)->m_dgId);
		// Copy the raw FICH
		fich.getRaw(buffer + 7U);

		if (ptr->m_debug)
			CUtils::dump(1U, "IMRS Network Data Sent", buffer, 165U);

		m_socket.write(buffer, 165U, (*it)->m_address, IMRS_PORT);
	}
}

void CIMRSNetwork::link()
{
}

void CIMRSNetwork::unlink()
{
}

void CIMRSNetwork::clock(unsigned int ms)
{
	unsigned char buffer[500U];

	in_addr address;
	unsigned int port;
	int length = m_socket.read(buffer, 500U, address, port);
	if (length <= 0)
		return;

	LogDebug("IMRS Network Data Reecived from port %u", port);
	CUtils::dump(1U, "IMRS Network Data Received", buffer, length);
	return;

	if (port != IMRS_PORT)
		return;

	IMRSDGId* ptr = find(address);
	if (ptr == NULL)
		return;

	if (ptr->m_debug)
		CUtils::dump(1U, "IMRS Network Data Received", buffer, length);

	unsigned char len = length;
	ptr->m_buffer.addData(&len, 1U);
	ptr->m_buffer.addData(buffer, len);
}

unsigned int CIMRSNetwork::read(unsigned int dgId, unsigned char* data)
{
	assert(data != NULL);

	IMRSDGId* ptr = find(dgId);
	if (ptr == NULL)
		return 0U;

	if (ptr->m_buffer.isEmpty())
		return 0U;

	unsigned char len = 0U;
	ptr->m_buffer.getData(&len, 1U);

	ptr->m_buffer.getData(data, len);

	return len;
}

void CIMRSNetwork::close()
{
	LogMessage("Closing IMRS network connection");

	m_socket.close();
}

IMRSDGId* CIMRSNetwork::find(in_addr address) const
{
	for (std::vector<IMRSDGId*>::const_iterator it1 = m_dgIds.begin(); it1 != m_dgIds.end(); ++it1) {
		for (std::vector<IMRSDest*>::const_iterator it2 = (*it1)->m_destinations.begin(); it2 != (*it1)->m_destinations.end(); ++it2) {
			if (address.s_addr == (*it2)->m_address.s_addr)
				return *it1;
		}
	}

	return NULL;
}

IMRSDGId* CIMRSNetwork::find(unsigned int dgId) const
{
	for (std::vector<IMRSDGId*>::const_iterator it = m_dgIds.begin(); it != m_dgIds.end(); ++it) {
		if (dgId == (*it)->m_dgId)
			return *it;
	}

	return NULL;
}

