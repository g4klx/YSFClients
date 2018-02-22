/*
 *   Copyright (C) 2009-2014,2016,2017,2018 by Jonathan Naylor G4KLX
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
#include "FCSNetwork.h"
#include "Utils.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>

const char* FCS_VERSION = "MMDVM v.01";

const unsigned int BUFFER_LENGTH = 200U;

CFCSNetwork::CFCSNetwork(unsigned int port, const std::string& callsign, unsigned int rxFrequency, unsigned int txFrequency, const std::string& locator, unsigned int id, bool debug) :
m_socket(port),
m_debug(debug),
m_address(),
m_port(0U),
m_ping(NULL),
m_info(NULL),
m_reflector(),
m_buffer(1000U, "FCS Network Buffer"),
m_n(0U),
m_pingTimer(1000U, 5U),
m_state(FCS_UNLINKED)
{
	m_info = new unsigned char[100U];
	::sprintf((char*)m_info, "%9u%9u%-6.6s%-12.12s%7u", rxFrequency, txFrequency, locator.c_str(), FCS_VERSION, id);

	m_ping = new unsigned char[25U];
	::sprintf((char*)m_ping, "PING%6.6s        ", callsign.c_str());
}

CFCSNetwork::~CFCSNetwork()
{
	delete[] m_info;
	delete[] m_ping;
}

bool CFCSNetwork::open()
{
	LogMessage("Resolving FCS00x addresses");

	m_addresses["FCS001"] = CUDPSocket::lookup("fcs001.xreflector.net");
	m_addresses["FCS002"] = CUDPSocket::lookup("fcs002.xreflector.net");
	m_addresses["FCS003"] = CUDPSocket::lookup("fcs003.xreflector.net");
	m_addresses["FCS004"] = CUDPSocket::lookup("fcs004.xreflector.net");

	LogMessage("Opening FCS network connection");

	return m_socket.open();
}

void CFCSNetwork::clearDestination()
{
	m_address.s_addr = INADDR_NONE;
	m_port           = 0U;

	m_pingTimer.stop();

	m_state = FCS_UNLINKED;
}

void CFCSNetwork::write(const unsigned char* data)
{
	assert(data != NULL);

	if (m_port == 0U)
		return;

	if (m_state != FCS_LINKED)
		return;

	unsigned char buffer[130U];
	::memset(buffer + 0U, ' ', 130U);
	::memcpy(buffer + 0U, data + 35U, 120U);
	::memcpy(buffer + 121U, m_reflector.c_str(), 8U);

	if (m_debug)
		CUtils::dump(1U, "FCS Network Data Sent", buffer, 130U);

	m_socket.write(buffer, 130U, m_address, m_port);
}

void CFCSNetwork::writeLink(const std::string& reflector)
{
	if (m_port == 0U) {
		std::string name = reflector.substr(0U, 6U);
		if (m_addresses.count(name) == 0U) {
			LogError("Unknown FCS reflector - %s", name.c_str());
			return;
		}

		m_address = m_addresses[name];
		if (m_address.s_addr == INADDR_NONE) {
			LogError("FCS reflector %s has no address", name.c_str());
			return;
		}
	}

	m_port = FCS_PORT;

	m_reflector = reflector;

	::memcpy(m_ping + 10U, m_reflector.c_str(), 8U);

	writePing();

	m_pingTimer.start();

	m_state = FCS_LINKING;
}

void CFCSNetwork::writeUnlink(unsigned int count)
{
	if (m_port == 0U)
		return;

	for (unsigned int i = 0U; i < count; i++)
		m_socket.write((unsigned char*)"CLOSE      ", 11U, m_address, m_port);

	m_pingTimer.stop();

	m_state = FCS_UNLINKED;
}

void CFCSNetwork::clock(unsigned int ms)
{
	unsigned char buffer[BUFFER_LENGTH];

	in_addr address;
	unsigned int port;
	int length = m_socket.read(buffer, BUFFER_LENGTH, address, port);
	if (length <= 0)
		return;

	if (m_port == 0U)
		return;

	m_pingTimer.clock(ms);
	if (m_pingTimer.isRunning() && m_pingTimer.hasExpired()) {
		writePing();
		m_pingTimer.start();
	}

	if (address.s_addr != m_address.s_addr || port != m_port)
		return;

	if (m_debug)
		CUtils::dump(1U, "FCS Network Data Received", buffer, length);

	if (length == 7) {
		m_state = FCS_LINKED;
		writeInfo();
	}

	if (length == 10 && m_state == FCS_LINKING) {
		m_state = FCS_LINKED;
		writeInfo();
	}

	if (length == 10 || length == 130) {
		unsigned char len = length;
		m_buffer.addData(&len, 1U);
		m_buffer.addData(buffer, len);
	}
}

unsigned int CFCSNetwork::read(unsigned char* data)
{
	assert(data != NULL);

	if (m_buffer.isEmpty())
		return 0U;

	unsigned char len = 0U;
	m_buffer.getData(&len, 1U);

	// Pass pings up to the gateway to reset the lost timer.
	if (len == 10U) {
		m_buffer.getData(data, len);
		return 10U;
	}

	unsigned char buffer[130U];
	m_buffer.getData(buffer, len);

	::memcpy(data + 0U, "YSFDDB0SAT    DB0SAT-RPTALL        ", 35U);
	::memcpy(data + 35U, buffer, 120U);

	// Put the reflector name as the via callsign.
	::memcpy(data + 4U, buffer + 121U, 8U);

	data[34U] = m_n;
	m_n += 2U;

	return 155U;
}

void CFCSNetwork::close()
{
	m_socket.close();

	LogMessage("Closing FCS network connection");
}

void CFCSNetwork::writeInfo()
{
	if (m_port == 0U)
		return;

	if (m_debug)
		CUtils::dump(1U, "FCS Network Data Sent", m_info, 100U);

	m_socket.write(m_info, 100U, m_address, m_port);
}

void CFCSNetwork::writePing()
{
	if (m_port == 0U)
		return;

	if (m_debug)
		CUtils::dump(1U, "FCS Network Data Sent", m_ping, 25U);

	m_socket.write(m_ping, 25U, m_address, m_port);
}
