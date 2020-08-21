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
#include "YSFNetwork.h"
#include "Utils.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>

const unsigned int BUFFER_LENGTH = 200U;

CYSFNetwork::CYSFNetwork(const std::string& localAddress, unsigned int localPort, const std::string& name, const in_addr& address, unsigned int port, const std::string& callsign, bool debug) :
m_socket(localAddress, localPort),
m_debug(debug),
m_address(address),
m_port(port),
m_poll(NULL),
m_unlink(NULL),
m_buffer(1000U, "YSF Network Buffer"),
m_pollTimer(1000U, 5U),
m_name(name),
m_linked(true)
{
	m_poll = new unsigned char[14U];
	::memcpy(m_poll + 0U, "YSFP", 4U);

	m_unlink = new unsigned char[14U];
	::memcpy(m_unlink + 0U, "YSFU", 4U);

	std::string node = callsign;
	node.resize(YSF_CALLSIGN_LENGTH, ' ');

	for (unsigned int i = 0U; i < YSF_CALLSIGN_LENGTH; i++) {
		m_poll[i + 4U]   = node.at(i);
		m_unlink[i + 4U] = node.at(i);
	}
}

CYSFNetwork::CYSFNetwork(unsigned int localPort, const std::string& name, const in_addr& address, unsigned int port, const std::string& callsign, bool debug) :
m_socket(localPort),
m_debug(debug),
m_address(address),
m_port(port),
m_poll(NULL),
m_unlink(NULL),
m_buffer(1000U, "YSF Network Buffer"),
m_pollTimer(1000U, 5U),
m_name(name),
m_linked(false)
{
	m_poll = new unsigned char[14U];
	::memcpy(m_poll + 0U, "YSFP", 4U);

	m_unlink = new unsigned char[14U];
	::memcpy(m_unlink + 0U, "YSFU", 4U);

	std::string node = callsign;
	node.resize(YSF_CALLSIGN_LENGTH, ' ');

	for (unsigned int i = 0U; i < YSF_CALLSIGN_LENGTH; i++) {
		m_poll[i + 4U]   = node.at(i);
		m_unlink[i + 4U] = node.at(i);
	}
}

CYSFNetwork::~CYSFNetwork()
{
	delete[] m_poll;
}

std::string CYSFNetwork::getDesc(unsigned int dgId)
{
	return "YSF: " + m_name;
}

bool CYSFNetwork::open()
{
	LogMessage("Opening YSF network connection");

	return m_socket.open();
}

void CYSFNetwork::write(unsigned int dgid, const unsigned char* data)
{
	assert(data != NULL);

	if (!m_linked)
		return;

	if (m_debug)
		CUtils::dump(1U, "YSF Network Data Sent", data, 155U);

	m_socket.write(data, 155U, m_address, m_port);
}

void CYSFNetwork::link()
{
	writePoll();
}

void CYSFNetwork::writePoll()
{
	m_pollTimer.start();

	m_socket.write(m_poll, 14U, m_address, m_port);
}

void CYSFNetwork::unlink()
{
	m_pollTimer.stop();

	m_socket.write(m_unlink, 14U, m_address, m_port);

	m_linked = false;
}

void CYSFNetwork::clock(unsigned int ms)
{
	unsigned char buffer[BUFFER_LENGTH];
	in_addr address;
	unsigned int port;

	m_pollTimer.clock(ms);
	if (m_pollTimer.isRunning() && m_pollTimer.hasExpired())
		writePoll();

	int length = m_socket.read(buffer, BUFFER_LENGTH, address, port);
	if (length <= 0)
		return;

	if (m_port == 0U)
		return;

	if (address.s_addr != m_address.s_addr || port != m_port)
		return;

	if (::memcmp(buffer, "YSFP", 4U) == 0 && !m_linked) {
		if (strcmp(m_name.c_str(),"MMDVM")== 0)
			LogMessage("Link successful to %s", m_name.c_str());
		else
			LogMessage("Linked to %s", m_name.c_str());

		m_linked = true;
	}

	if (m_debug)
		CUtils::dump(1U, "YSF Network Data Received", buffer, length);

	unsigned char len = length;
	m_buffer.addData(&len, 1U);

	m_buffer.addData(buffer, length);
}

unsigned int CYSFNetwork::read(unsigned int dgid, unsigned char* data)
{
	assert(data != NULL);

	if (m_buffer.isEmpty())
		return 0U;

	unsigned char len = 0U;
	m_buffer.getData(&len, 1U);

	m_buffer.getData(data, len);

	return len;
}

void CYSFNetwork::close()
{
	m_socket.close();

	LogMessage("Closing YSF network connection");
}
