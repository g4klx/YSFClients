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

#include "YSFDefines.h"
#include "YCSNetwork.h"
#include "Utils.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>

const unsigned int BUFFER_LENGTH = 200U;

CYCSNetwork::CYCSNetwork(unsigned int localPort, const std::string& name, const sockaddr_storage& addr, unsigned int addrLen, const std::string& callsign, unsigned int rxFrequency, unsigned int txFrequency, const std::string& locator, const std::string& description, unsigned int id, unsigned int dgId, bool statc, bool debug) :
m_socket(localPort),
m_debug(debug),
m_addr(addr),
m_addrLen(addrLen),
m_static(statc),
m_poll(NULL),
m_options(NULL),
m_info(NULL),
m_unlink(NULL),
m_buffer(1000U, "YCS Network Buffer"),
m_sendPollTimer(1000U, 5U),
m_recvPollTimer(1000U, 60U),
m_name(name),
m_state(DS_NOTOPEN),
m_dgId(dgId)
{
	m_poll = new unsigned char[14U];
	::memcpy(m_poll + 0U, "YSFP", 4U);

	m_unlink = new unsigned char[14U];
	::memcpy(m_unlink + 0U, "YSFU", 4U);

	m_options = new unsigned char[50U];
	::memcpy(m_options + 0U, "YSFO", 4U);

	m_info = new unsigned char[80U];
	::memcpy(m_info + 0U, "YSFI", 4U);

	std::string node = callsign;
	node.resize(YSF_CALLSIGN_LENGTH, ' ');

	for (unsigned int i = 0U; i < YSF_CALLSIGN_LENGTH; i++) {
		m_poll[i + 4U]    = node.at(i);
		m_unlink[i + 4U]  = node.at(i);
		m_options[i + 4U] = node.at(i);
		m_info[i + 4U]    = node.at(i);
	}

	char text[101U];
	::sprintf(text, "%u                                                           ", dgId);

	for (unsigned int i = 0U; i < (50U - 4U - YSF_CALLSIGN_LENGTH); i++)
		m_options[i + 4U + YSF_CALLSIGN_LENGTH] = text[i];

	std::string desc = description;
	desc.resize(20U, ' ');

	sprintf(text, "%9u%9u%.6s%sMMDVM       %07u   ", rxFrequency, txFrequency, locator.c_str(), desc.c_str(), id);
	for (unsigned int i = 0U; i < (80U - 4U - YSF_CALLSIGN_LENGTH); i++)
		m_info[i + 4U + YSF_CALLSIGN_LENGTH] = text[i];
}

CYCSNetwork::~CYCSNetwork()
{
	delete[] m_poll;
	delete[] m_unlink;
	delete[] m_options;
	delete[] m_info;
}

std::string CYCSNetwork::getDesc(unsigned int dgId)
{
	return "YCS: " + m_name;
}

unsigned int CYCSNetwork::getDGId()
{
	return m_dgId;
}

bool CYCSNetwork::open()
{
	if (m_addrLen == 0U) {
		LogError("Unable to resolve the address of the YCS network");
		m_state = DS_NOTOPEN;
		return false;
	}

	LogMessage("Opening YCS network connection");

	bool ret = m_socket.open(m_addr);
	if (!ret) {
		m_state = DS_NOTOPEN;
		return false;
	} else {
		m_state = DS_NOTLINKED;
		return true;
	}
}

DGID_STATUS CYCSNetwork::getStatus()
{
	return m_state;
}

void CYCSNetwork::write(unsigned int dgid, const unsigned char* data)
{
	assert(data != NULL);

	if (m_state != DS_LINKED)
		return;

	if (m_debug)
		CUtils::dump(1U, "YCS Network Data Sent", data, 155U);

	m_socket.write(data, 155U, m_addr, m_addrLen);
}

void CYCSNetwork::link()
{
	if (m_state != DS_NOTLINKED)
		return;

	m_state = DS_LINKING;

	m_sendPollTimer.start();
	m_recvPollTimer.start();

	writePoll();
}

void CYCSNetwork::writePoll()
{
	if (m_state != DS_LINKING && m_state != DS_LINKED)
		return;

	if (m_debug)
		CUtils::dump(1U, "YCS Network Data Sent", m_poll, 14U);

	m_socket.write(m_poll, 14U, m_addr, m_addrLen);
}

void CYCSNetwork::unlink()
{
	if (m_state != DS_LINKED)
		return;

	m_sendPollTimer.stop();
	m_recvPollTimer.stop();

	if (m_debug)
		CUtils::dump(1U, "YCS Network Data Sent", m_unlink, 14U);

	m_socket.write(m_unlink, 14U, m_addr, m_addrLen);

	LogMessage("Unlinked from %s", m_name.c_str());

	m_state = DS_NOTLINKED;
}

void CYCSNetwork::clock(unsigned int ms)
{
	if (m_state == DS_NOTOPEN)
		return;

	m_recvPollTimer.clock(ms);
	if (m_recvPollTimer.isRunning() && m_recvPollTimer.hasExpired()) {
		if (m_static) {
			m_state = DS_LINKING;
		} else {
			m_state = DS_NOTLINKED;
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
		CUtils::dump(1U, "YCS Network Data Received", buffer, length);

	if (::memcmp(buffer, "YSFP", 4U) == 0) {
		m_recvPollTimer.start();

		if (m_state == DS_LINKING) {
			LogMessage("Linked to %s", m_name.c_str());

			m_state = DS_LINKED;

			if (m_debug)
				CUtils::dump(1U, "YCS Network Data Sent", m_options, 50U);

			m_socket.write(m_options, 50U, m_addr, m_addrLen);

			if (m_debug)
				CUtils::dump(1U, "YCS Network Data Sent", m_info, 80U);

			m_socket.write(m_info, 80U, m_addr, m_addrLen);
		}
	}

	if (::memcmp(buffer, "YSFD", 4U) == 0) {
		m_recvPollTimer.start();

		unsigned char len = length;
		m_buffer.addData(&len, 1U);

		m_buffer.addData(buffer, length);
	}
}

unsigned int CYCSNetwork::read(unsigned int dgid, unsigned char* data)
{
	assert(data != NULL);

	if (m_buffer.isEmpty())
		return 0U;

	unsigned char len = 0U;
	m_buffer.getData(&len, 1U);

	m_buffer.getData(data, len);

	return len;
}

void CYCSNetwork::close()
{
	m_socket.close();

	LogMessage("Closing YCS network connection");

	m_state = DS_NOTOPEN;
}
