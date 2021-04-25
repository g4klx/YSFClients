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

#include "YSFDefines.h"
#include "Network.h"
#include "Utils.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>

CNetwork::CNetwork(unsigned short port, unsigned int id, const std::string& name, const std::string& description, bool debug) :
m_socket(),
m_port(port),
m_id(id),
m_name(name),
m_description(description),
m_callsign(),
m_debug(debug),
m_status(NULL)
{
	m_name.resize(16U, ' ');
	m_description.resize(14U, ' ');

	m_status = new unsigned char[50U];
}

CNetwork::~CNetwork()
{
	delete[] m_status;
}

bool CNetwork::open()
{
	LogInfo("Opening YSF network connection");

	bool status = false;
	unsigned int af[] = {AF_INET, AF_INET6};

	for (unsigned int i = 0U; i < UDP_SOCKET_MAX && i < (sizeof(af) / sizeof(unsigned int)); i++)
		status |= m_socket.open(i, af[i], "", m_port);

	return status;
}

bool CNetwork::writeData(const unsigned char* data, const sockaddr_storage& addr, unsigned int addrLen)
{
	assert(data != NULL);

	if (m_debug)
		CUtils::dump(1U, "YSF Network Data Sent", data, 155U);

	return m_socket.write(data, 155U, addr, addrLen);
}

bool CNetwork::writePoll(const sockaddr_storage& addr, unsigned int addrLen)
{
	unsigned char buffer[20U];

	buffer[0] = 'Y';
	buffer[1] = 'S';
	buffer[2] = 'F';
	buffer[3] = 'P';

	buffer[4U]  = 'R';
	buffer[5U]  = 'E';
	buffer[6U]  = 'F';
	buffer[7U]  = 'L';
	buffer[8U]  = 'E';
	buffer[9U]  = 'C';
	buffer[10U] = 'T';
	buffer[11U] = 'O';
	buffer[12U] = 'R';
	buffer[13U] = ' ';

	if (m_debug)
		CUtils::dump(1U, "YSF Network Poll Sent", buffer, 14U);

	return m_socket.write(buffer, 14U, addr, addrLen);
}

unsigned int CNetwork::readData(unsigned char* data, unsigned int length, sockaddr_storage& addr, unsigned int& addrLen)
{
	assert(data != NULL);
	assert(length > 0U);

	int len = m_socket.read(data, length, addr, addrLen);
	if (len <= 0)
		return 0U;

	if (m_debug)
		CUtils::dump(1U, "YSF Network Data Received", data, len);

	// Throw away any options messages
	if (::memcmp(data, "YSFO", 4U) == 0)
		return 0U;

	// Throw away any info messages
	if (::memcmp(data, "YSFI", 4U) == 0)
		return 0U;

	// Handle incoming status requests
	if (::memcmp(data, "YSFS", 4U) == 0) {
		m_socket.write(m_status, 42U, addr, addrLen);
		return 0U;
	}

	return len;
}

void CNetwork::setCount(unsigned int count)
{
	if (count > 999U)
		count = 999U;

	unsigned int hash = m_id;

	if (hash == 0U) {
		for (unsigned int i = 0U; i < m_name.size(); i++) {
			hash += m_name.at(i);
			hash += (hash << 10);
			hash ^= (hash >> 6);
		}

		// Final avalanche
		hash += (hash << 3);
		hash ^= (hash >> 11);
		hash += (hash << 15);
	}

	::sprintf((char*)m_status, "YSFS%05u%16.16s%14.14s%03u", hash % 100000U, m_name.c_str(), m_description.c_str(), count);
}

void CNetwork::close()
{
	m_socket.close();

	LogInfo("Closing YSF network connection");
}
