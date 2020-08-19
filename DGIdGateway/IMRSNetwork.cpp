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

bool CIMRSNetwork::open()
{
	LogMessage("Opening IMRS network connection");

	return m_socket.open();
}

void CIMRSNetwork::write(unsigned int dgId, const unsigned char* data)
{
	assert(data != NULL);

	IMRSDGId* ptr = find(dgId);
	if (ptr == NULL)
		return;

	unsigned char buffer[200U];

	for (std::vector<IMRSDest*>::const_iterator it = ptr->m_destinations.begin(); it != ptr->m_destinations.end(); ++it) {
		if (ptr->m_debug)
			CUtils::dump(1U, "IMRS Network Data Sent", buffer, 130U);

		m_socket.write(buffer, 130U, (*it)->m_address, IMRS_PORT);
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

