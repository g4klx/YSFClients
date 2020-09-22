/*
 *   Copyright (C) 2009-2014,2016,2018,2020 by Jonathan Naylor G4KLX
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

CNetwork::CNetwork(unsigned int port) :
m_socket(port),
m_address(),
m_port(0U)
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
	if (m_port == 0U)
		return true;

	assert(data != NULL);

	return m_socket.write(data, 155U, m_address, m_port);
}

bool CNetwork::writePoll(const in_addr& address, unsigned int port)
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

	return m_socket.write(buffer, 14U, address, port);
}

unsigned int CNetwork::read(unsigned char* data)
{
	in_addr address;
	unsigned int port;
	int length = m_socket.read(data, BUFFER_LENGTH, address, port);
	if (length <= 0)
		return 0U;

	// Handle incoming polls
	if (::memcmp(data, "YSFP", 4U) == 0) {
		writePoll(address, port);
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
		m_socket.write(status, 42U, address, port);
		return 0U;
	}

	// Invalid packet type?
	if (::memcmp(data, "YSFD", 4U) != 0)
		return 0U;

	m_address.s_addr = address.s_addr;
	m_port = port;

	return 155U;
}

void CNetwork::end()
{
	m_port = 0U;
}

void CNetwork::close()
{
	m_socket.close();

	::fprintf(stdout, "Closing YSF network connection\n");
}
