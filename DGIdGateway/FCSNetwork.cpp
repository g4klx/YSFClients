/*
 *   Copyright (C) 2009-2014,2016,2017,2018,2020,2021,2025 by Jonathan Naylor G4KLX
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

const char* FCS_VERSION = "MMDVM";

const unsigned int BUFFER_LENGTH = 200U;

CFCSNetwork::CFCSNetwork(const std::string& reflector, unsigned short port, const std::string& callsign, unsigned int rxFrequency, unsigned int txFrequency, const std::string& locator, unsigned int id, bool statc, bool debug) :
m_socket(port),
m_debug(debug),
m_addr(),
m_addrLen(0U),
m_static(statc),
m_ping(nullptr),
m_info(nullptr),
m_reflector(reflector),
m_print(),
m_buffer(1000U, "FCS Network Buffer"),
m_n(0U),
m_sendPollTimer(1000U, 0U, 800U),
m_recvPollTimer(1000U, 60U),
m_resetTimer(1000U, 1U),
m_state(DGID_STATUS::NOTOPEN)
{
	m_info = new unsigned char[100U];
	::sprintf((char*)m_info, "%9u%9u%-6.6s%-12.12s%7u", rxFrequency, txFrequency, locator.c_str(), FCS_VERSION, id);
	::memset(m_info + 43U, ' ', 57U);

	m_ping = new unsigned char[25U];
	::memcpy(m_ping + 0U, "PING", 4U);
	::memset(m_ping + 4U, ' ', 6U);
	::memcpy(m_ping + 4U, callsign.c_str(), callsign.size());
	::memset(m_ping + 10U, 0x00U, 15U);
	::memcpy(m_ping + 10U, reflector.c_str(), 8U);

	m_print = reflector.substr(0U, 6U) + "-" + reflector.substr(6U);

	char url[50U];
	::sprintf(url, "%.6s.xreflector.net", reflector.c_str());
	if (CUDPSocket::lookup(std::string(url), FCS_PORT, m_addr, m_addrLen) != 0)
		m_addrLen = 0U;
}

CFCSNetwork::~CFCSNetwork()
{
	delete[] m_info;
	delete[] m_ping;
}

std::string CFCSNetwork::getDesc(unsigned int dgId)
{
	return "FCS: " + m_reflector;
}

unsigned int CFCSNetwork::getDGId()
{
	return 0U;
}

bool CFCSNetwork::open()
{
	if (m_addrLen == 0U) {
		LogError("Unable to resolve the address of %s", m_reflector.c_str());
		m_state = DGID_STATUS::NOTOPEN;
		return false;
	}

	LogMessage("Opening FCS network connection");

	bool ret = m_socket.open(m_addr);
	if (!ret) {
		m_state = DGID_STATUS::NOTOPEN;
		return false;
	} else {
		m_state = DGID_STATUS::NOTLINKED;
		return true;
	}
}

DGID_STATUS CFCSNetwork::getStatus()
{
	return m_state;
}

void CFCSNetwork::write(unsigned int dgid, const unsigned char* data)
{
	assert(data != nullptr);

	if (m_state != DGID_STATUS::LINKED)
		return;

	unsigned char buffer[130U];
	::memset(buffer + 0U, ' ', 130U);
	::memcpy(buffer + 0U, data + 35U, 120U);
	::memcpy(buffer + 120U, data + 34U, 1U);
	::memcpy(buffer + 121U, m_reflector.c_str(), 8U);

	if (m_debug)
		CUtils::dump(1U, "FCS Network Data Sent", buffer, 130U);

	m_socket.write(buffer, 130U, m_addr, m_addrLen);
}

void CFCSNetwork::link()
{
	if (m_state != DGID_STATUS::NOTLINKED)
		return;

	m_state = DGID_STATUS::LINKING;

	m_sendPollTimer.start();
	m_recvPollTimer.start();

	writePoll();
}

void CFCSNetwork::unlink()
{
	if (m_state != DGID_STATUS::LINKED)
		return;

	m_socket.write((unsigned char*)"CLOSE      ", 11U, m_addr, m_addrLen);

	m_sendPollTimer.stop();
	m_recvPollTimer.stop();

	LogMessage("Unlinked from %s", m_print.c_str());

	m_state = DGID_STATUS::NOTLINKED;
}

void CFCSNetwork::clock(unsigned int ms)
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

		LogMessage("Lost link to %s", m_print.c_str());
		m_recvPollTimer.stop();
	}

	m_sendPollTimer.clock(ms);
	if (m_sendPollTimer.isRunning() && m_sendPollTimer.hasExpired()) {
		writePoll();
		m_sendPollTimer.start();
	}

	m_resetTimer.clock(ms);
	if (m_resetTimer.isRunning() && m_resetTimer.hasExpired()) {
		m_n = 0U;
		m_resetTimer.stop();
	}

	unsigned char buffer[BUFFER_LENGTH];

	sockaddr_storage addr;
	unsigned int addrLen;
	int length = m_socket.read(buffer, BUFFER_LENGTH, addr, addrLen);
	if (length <= 0)
		return;

	if (m_debug)
		CUtils::dump(1U, "FCS Network Data Received", buffer, length);

	if (m_state == DGID_STATUS::NOTLINKED)
		return;

	if (!CUDPSocket::match(addr, m_addr))
		return;

	if (m_debug)
		CUtils::dump(1U, "FCS Network Data Received", buffer, length);

	if (length == 7 || length == 10) {
		m_recvPollTimer.start();

		if (m_state == DGID_STATUS::LINKING) {
			LogMessage("Linked to %s", m_print.c_str());

			m_state = DGID_STATUS::LINKED;

			if (m_debug)
				CUtils::dump(1U, "FCS Network Data Sent", m_info, 100U);

			m_socket.write(m_info, 100U, m_addr, m_addrLen);
		}
	}

	if (length == 130) {
		m_recvPollTimer.start();

		unsigned char len = length;
		m_buffer.addData(&len, 1U);
		m_buffer.addData(buffer, len);
	}
}

unsigned int CFCSNetwork::read(unsigned int dgid, unsigned char* data)
{
	assert(data != nullptr);

	if (m_buffer.isEmpty())
		return 0U;

	unsigned char len = 0U;
	m_buffer.getData(&len, 1U);

	m_resetTimer.start();

	unsigned char buffer[130U];
	m_buffer.getData(buffer, len);

	::memset(data + 0U, ' ', 35U);
	::memcpy(data + 0U, "YSFD", 4U);
	::memcpy(data + 35U, buffer, 120U);

	// Put the reflector name as the via callsign.
	::memcpy(data + 4U, m_print.c_str(), 9U);

	data[34U] = m_n;
	m_n += 2U;

	return 155U;
}

void CFCSNetwork::close()
{
	m_socket.close();

	LogMessage("Closing FCS network connection");

	m_state = DGID_STATUS::NOTOPEN;
}

void CFCSNetwork::writePoll()
{
	if (m_state != DGID_STATUS::LINKING && m_state != DGID_STATUS::LINKED)
		return;

	if (m_debug)
		CUtils::dump(1U, "FCS Network Data Sent", m_ping, 25U);

	m_socket.write(m_ping, 25U, m_addr, m_addrLen);
}
