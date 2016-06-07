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
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>

const unsigned int BUFFER_LENGTH = 200U;

CNetwork::CNetwork(const std::string& address, unsigned int port, bool debug) :
m_socket(address, port),
m_debug(debug),
m_address(),
m_port(0U),
m_buffer(1000U, "YSF Network Buffer"),
m_timer(1000U, 5U)
{
	assert(port > 0U);
}

CNetwork::CNetwork(unsigned int port, bool debug) :
m_socket(port),
m_debug(debug),
m_address(),
m_port(0U),
m_buffer(1000U, "YSF Network Buffer"),
m_timer(1000U, 5U)
{
	assert(port > 0U);
}

CNetwork::~CNetwork()
{
}

bool CNetwork::open()
{
	LogMessage("Opening YSF network connection");

	return m_socket.open();
}

void CNetwork::setDestination(const in_addr& address, unsigned int port)
{
	m_address = address;
	m_port    = port;

	m_timer.start();
}

void CNetwork::setDestination()
{
	m_address.s_addr = INADDR_NONE;
	m_port           = 0U;

	m_timer.stop();
}

bool CNetwork::write(const unsigned char* data)
{
	assert(data != NULL);

	if (m_port == 0U)
		return true;

	if (m_debug)
		CUtils::dump(1U, "YSF Network Data Sent", data, 155U);

	return m_socket.write(data, 155U, m_address, m_port);
}

bool CNetwork::writePoll()
{
	if (m_port == 0U)
		return true;

	unsigned char buffer[20U];

	buffer[0] = 'Y';
	buffer[1] = 'S';
	buffer[2] = 'F';
	buffer[3] = 'P';

	buffer[4U]  = 'G';
	buffer[5U]  = 'A';
	buffer[6U]  = 'T';
	buffer[7U]  = 'E';
	buffer[8U]  = 'W';
	buffer[9U]  = 'A';
	buffer[10U] = 'Y';
	buffer[11U] = ' ';
	buffer[12U] = ' ';
	buffer[13U] = ' ';

	if (m_debug)
		CUtils::dump(1U, "YSF Network Poll Sent", buffer, 14U);

	return m_socket.write(buffer, 14U, m_address, m_port);
}

void CNetwork::clock(unsigned int ms)
{
	m_timer.clock(ms);
	if (m_timer.isRunning() && m_timer.hasExpired()) {
		writePoll();
		m_timer.start();
	}

	unsigned char buffer[BUFFER_LENGTH];

	in_addr address;
	unsigned int port;
	int length = m_socket.read(buffer, BUFFER_LENGTH, address, port);
	if (length <= 0)
		return;

	if (address.s_addr != m_address.s_addr || port != m_port) {
		LogDebug("Addr: %u != %u || Port: %u != %u", address.s_addr, m_address.s_addr, port, m_port);
		CUtils::dump("Data from unknown address/port", buffer, length);
		return;
	}

	// Handle incoming polls
	if (::memcmp(buffer, "YSFP", 4U) == 0) {
		// XXX How to handle lost polls?
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

	LogMessage("Closing YSF network connection");
}
