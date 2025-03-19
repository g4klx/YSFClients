/*
 *   Copyright (C) 2009-2014,2016,2017,2018,2020,2025 by Jonathan Naylor G4KLX
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

CYSFNetwork::CYSFNetwork(const std::string& localAddress, unsigned short localPort, const std::string& name, const sockaddr_storage& addr, unsigned int addrLen, const std::string& callsign, bool debug) :
m_socket(localAddress, localPort),
m_debug(debug),
m_addr(addr),
m_addrLen(addrLen),
m_static(true),
m_poll(nullptr),
m_unlink(nullptr),
m_buffer(1000U, "YSF Network Buffer"),
m_sendPollTimer(1000U, 5U),
m_recvPollTimer(1000U, 60U),
m_name(name),
m_state(DGID_STATUS::NOTOPEN)
{
	m_poll = new unsigned char[14U];
	::memcpy(m_poll + 0U, "YSFP", 4U);

	m_unlink = new unsigned char[14U];
	::memcpy(m_unlink + 0U, "YSFU", 4U);

	std::string node = callsign;
	node.resize(YSF_CALLSIGN_LENGTH, ' ');

	for (unsigned int i = 0U; i < YSF_CALLSIGN_LENGTH; i++) {
		m_poll[i + 4U]    = node.at(i);
		m_unlink[i + 4U]  = node.at(i);
	}
}

CYSFNetwork::CYSFNetwork(unsigned short localPort, const std::string& name, const sockaddr_storage& addr, unsigned int addrLen, const std::string& callsign, bool statc, bool debug) :
m_socket(localPort),
m_debug(debug),
m_addr(addr),
m_addrLen(addrLen),
m_static(statc),
m_poll(nullptr),
m_unlink(nullptr),
m_buffer(1000U, "YSF Network Buffer"),
m_sendPollTimer(1000U, 5U),
m_recvPollTimer(1000U, 60U),
m_name(name),
m_state(DGID_STATUS::NOTOPEN)
{
	m_poll = new unsigned char[14U];
	::memcpy(m_poll + 0U, "YSFP", 4U);

	m_unlink = new unsigned char[14U];
	::memcpy(m_unlink + 0U, "YSFU", 4U);

	std::string node = callsign;
	node.resize(YSF_CALLSIGN_LENGTH, ' ');

	for (unsigned int i = 0U; i < YSF_CALLSIGN_LENGTH; i++) {
		m_poll[i + 4U]    = node.at(i);
		m_unlink[i + 4U]  = node.at(i);
	}
}

CYSFNetwork::~CYSFNetwork()
{
	delete[] m_poll;
	delete[] m_unlink;
}

std::string CYSFNetwork::getDesc(unsigned int dgId)
{
	return "YSF: " + m_name;
}

unsigned int CYSFNetwork::getDGId()
{
	return 0U;
}

bool CYSFNetwork::open()
{
	if (m_addrLen == 0U) {
		LogError("Unable to resolve the address of the YSF network");
		m_state = DGID_STATUS::NOTOPEN;
		return false;
	}

	LogMessage("Opening YSF network connection");

	bool ret = m_socket.open(m_addr);
	if (!ret) {
		m_state = DGID_STATUS::NOTOPEN;
		return false;
	} else {
		m_state = DGID_STATUS::NOTLINKED;
		return true;
	}
}

DGID_STATUS CYSFNetwork::getStatus()
{
	return m_state;
}

void CYSFNetwork::write(unsigned int dgid, const unsigned char* data)
{
	assert(data != nullptr);

	if (m_state != DGID_STATUS::LINKED)
		return;

	if (m_debug)
		CUtils::dump(1U, "YSF Network Data Sent", data, 155U);

	m_socket.write(data, 155U, m_addr, m_addrLen);
}

void CYSFNetwork::link()
{
	if (m_state != DGID_STATUS::NOTLINKED)
		return;

	m_state = DGID_STATUS::LINKING;

	m_sendPollTimer.start();
	m_recvPollTimer.start();

	writePoll();
}

void CYSFNetwork::writePoll()
{
	if (m_state != DGID_STATUS::LINKING && m_state != DGID_STATUS::LINKED)
		return;

	if (m_debug)
		CUtils::dump(1U, "YSF Network Data Sent", m_poll, 14U);

	m_socket.write(m_poll, 14U, m_addr, m_addrLen);
}

void CYSFNetwork::unlink()
{
	if (m_state != DGID_STATUS::LINKED)
		return;

	m_sendPollTimer.stop();
	m_recvPollTimer.stop();

	if (m_debug)
		CUtils::dump(1U, "YSF Network Data Sent", m_unlink, 14U);

	m_socket.write(m_unlink, 14U, m_addr, m_addrLen);

	LogMessage("Unlinked from %s", m_name.c_str());

	m_state = DGID_STATUS::NOTLINKED;
}

void CYSFNetwork::clock(unsigned int ms)
{
	if (m_state == DGID_STATUS::NOTOPEN)
		return;

	m_recvPollTimer.clock(ms);
	if (m_recvPollTimer.isRunning() && m_recvPollTimer.hasExpired()) {
		if (m_static) {
			m_state = DGID_STATUS::LINKING;
		} else {
			m_state = DGID_STATUS::NOTLINKED;
			m_sendPollTimer.stop();
		}

		LogMessage("Lost link to %s", m_name.c_str());
		m_recvPollTimer.stop();
	}

	m_sendPollTimer.clock(ms);
	if (m_sendPollTimer.isRunning() && m_sendPollTimer.hasExpired()) {
		writePoll();
		m_sendPollTimer.start();
	}

	unsigned char buffer[BUFFER_LENGTH];
	sockaddr_storage addr;
	unsigned int addrLen;
	int length = m_socket.read(buffer, BUFFER_LENGTH, addr, addrLen);
	if (length <= 0)
		return;

	if (m_addrLen == 0U)
		return;

	if (!CUDPSocket::match(addr, m_addr))
		return;

	if (m_debug)
		CUtils::dump(1U, "YSF Network Data Received", buffer, length);

	if (::memcmp(buffer, "YSFP", 4U) == 0) {
		m_recvPollTimer.start();

		if (m_state == DGID_STATUS::LINKING) {
			if (strcmp(m_name.c_str(), "MMDVM") == 0)
				LogMessage("Link successful to %s", m_name.c_str());
			else
				LogMessage("Linked to %s", m_name.c_str());

			m_state = DGID_STATUS::LINKED;
		}
	}

	if (::memcmp(buffer, "YSFD", 4U) == 0) {
		m_recvPollTimer.start();

		unsigned char len = length;
		m_buffer.addData(&len, 1U);

		m_buffer.addData(buffer, length);
	}
}

unsigned int CYSFNetwork::read(unsigned int dgid, unsigned char* data)
{
	assert(data != nullptr);

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

	m_state = DGID_STATUS::NOTOPEN;
}
