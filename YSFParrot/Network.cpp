/*
 *   Copyright (C) 2009-2014,2016 by Jonathan Naylor G4KLX
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

#include <cstdio>
#include <cassert>
#include <cstring>

const unsigned int BUFFER_LENGTH = 200U;

CNetwork::CNetwork(unsigned int port, bool debug) :
m_socket(port),
m_address(),
m_port(0U),
m_debug(debug),
m_buffer(1000U, "YSF Network"),
m_pollTimer(1000U, 5U)
{
}

CNetwork::~CNetwork()
{
}

bool CNetwork::open()
{
	::fprintf(stdout, "Opening YSF network connection\n");

	if (m_address.s_addr == INADDR_NONE)
		return false;

	m_pollTimer.start();

	return m_socket.open();
}

bool CNetwork::write(const unsigned char* data)
{
	assert(data != NULL);

	if (m_debug)
		CUtils::dump(1U, "YSF Network Data Sent", data, 155U);

	return m_socket.write(data, 155U, m_address, m_port);
}

bool CNetwork::writePoll()
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

	if (m_debug)
		CUtils::dump(1U, "YSF Network Poll Sent", buffer, 14U);

	return m_socket.write(buffer, 14U, m_address, m_port);
}

void CNetwork::clock(unsigned int ms)
{
	m_pollTimer.clock(ms);
	if (m_pollTimer.hasExpired()) {
		writePoll();
		m_pollTimer.start();
	}

	unsigned char buffer[BUFFER_LENGTH];

	in_addr address;
	unsigned int port;
	int length = m_socket.read(buffer, BUFFER_LENGTH, address, port);
	if (length <= 0)
		return;

	m_address.s_addr = address.s_addr;
	m_port = port;

	// Ignore incoming polls
	if (::memcmp(buffer, "YSFP", 4U) == 0)
		return;

	// Handle the status command
	if (::memcmp(buffer, "YSFS", 4U) == 0) {
		unsigned char status[50U];
		::sprintf((char*)status, "YSFS%05u%-16.16s%-14.14s%03u", 1U, "Parrot", "Parrot", 0U);
		m_socket.write(status, 42U, address, port);
		return;
	}

	// Invalid packet type?
	if (::memcmp(buffer, "YSFD", 4U) != 0)
		return;

	if (m_debug)
		CUtils::dump(1U, "YSF Network Data Received", buffer, length);

	m_buffer.addData(buffer, 155U);
}

unsigned int CNetwork::read(unsigned char* data)
{
	assert(data != NULL);

	if (m_buffer.isEmpty())
		return 0U;

	m_buffer.getData(data, 155U);

	return 155U;
}

void CNetwork::close()
{
	m_socket.close();

	::fprintf(stdout, "Closing YSF network connection\n");
}
