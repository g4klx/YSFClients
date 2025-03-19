/*
 *   Copyright (C) 2009-2014,2016,2018,2020,2025 by Jonathan Naylor G4KLX
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

#include "Network.h"

#include <cstdio>
#include <cassert>
#include <cstring>

const unsigned int BUFFER_LENGTH = 200U;

CNetwork::CNetwork(unsigned short port) :
m_socket(port),
m_addr(),
m_addrLen(0U)
{
}

CNetwork::~CNetwork()
{
}

bool CNetwork::open()
{
	::fprintf(stdout, "Opening YSF network connection\n");

	return m_socket.open();
}

bool CNetwork::write(const unsigned char* data)
{
	if (m_addrLen == 0U)
		return true;

	assert(data != nullptr);

	return m_socket.write(data, 155U, m_addr, m_addrLen);
}

bool CNetwork::writePoll(const sockaddr_storage& addr, unsigned int addrLen)
{
	unsigned char buffer[20U];

	buffer[0] = 'Y';
	buffer[1] = 'S';
	buffer[2] = 'F';
	buffer[3] = 'P';

	buffer[4U]  = 'P';
	buffer[5U]  = 'A';
	buffer[6U]  = 'R';
	buffer[7U]  = 'R';
	buffer[8U]  = 'O';
	buffer[9U]  = 'T';
	buffer[10U] = ' ';
	buffer[11U] = ' ';
	buffer[12U] = ' ';
	buffer[13U] = ' ';

	return m_socket.write(buffer, 14U, addr, addrLen);
}

unsigned int CNetwork::read(unsigned char* data)
{
	sockaddr_storage addr;
	unsigned int addrLen;
	int length = m_socket.read(data, BUFFER_LENGTH, addr, addrLen);
	if (length <= 0)
		return 0U;

	// Handle incoming polls
	if (::memcmp(data, "YSFP", 4U) == 0) {
		writePoll(addr, addrLen);
		return 0U;
	}

	// Throw away incoming options messages
	if (::memcmp(data, "YSFO", 4U) == 0)
		return 0U;

	// Throw away incoming info messages
	if (::memcmp(data, "YSFI", 4U) == 0)
		return 0U;

	// Handle incoming unlinks
	if (::memcmp(data, "YSFU", 4U) == 0)
		return 0U;

	// Handle the status command
	if (::memcmp(data, "YSFS", 4U) == 0) {
		unsigned char status[50U];
		::sprintf((char*)status, "YSFS%05u%-16.16s%-14.14s%03u", 1U, "Parrot", "Parrot", 0U);
		m_socket.write(status, 42U, addr, addrLen);
		return 0U;
	}

	// Invalid packet type?
	if (::memcmp(data, "YSFD", 4U) != 0)
		return 0U;

	m_addr    = addr;
	m_addrLen = addrLen;

	return 155U;
}

void CNetwork::end()
{
	m_addrLen = 0U;
}

void CNetwork::close()
{
	m_socket.close();

	::fprintf(stdout, "Closing YSF network connection\n");
}
