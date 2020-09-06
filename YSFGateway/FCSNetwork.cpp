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

const char* FCS_VERSION = "MMDVM";

const unsigned int BUFFER_LENGTH = 200U;

CFCSNetwork::CFCSNetwork(unsigned int port, const std::string& callsign, unsigned int rxFrequency, unsigned int txFrequency, const std::string& locator, unsigned int id, bool debug) :
m_socket(port),
m_debug(debug),
m_address(),
m_ping(NULL),
m_options(NULL),
m_opt(),
m_info(NULL),
m_reflector(),
m_print(),
m_buffer(1000U, "FCS Network Buffer"),
m_n(0U),
m_pingTimer(1000U, 0U, 800U),
m_resetTimer(1000U, 1U),
m_state(FCS_UNLINKED)
{
	m_info = new unsigned char[100U];
	::sprintf((char*)m_info, "%9u%9u%-6.6s%-12.12s%7u", rxFrequency, txFrequency, locator.c_str(), FCS_VERSION, id);
	::memset(m_info + 43U, ' ', 57U);

	m_ping = new unsigned char[25U];
	::memcpy(m_ping + 0U, "PING", 4U);
	::memset(m_ping + 4U, ' ', 6U);
	::memcpy(m_ping + 4U, callsign.c_str(), callsign.size());
	::memset(m_ping + 10U, 0x00U, 15U);

	m_options = new unsigned char[50U];
	::memcpy(m_options + 0U, "FCSO", 4U);
	::memset(m_options + 4U, ' ', 46U);
	::memcpy(m_options + 4U, callsign.c_str(), callsign.size());
}

CFCSNetwork::~CFCSNetwork()
{
	delete[] m_info;
	delete[] m_ping;
	delete[] m_options;
}

bool CFCSNetwork::open()
{
	LogMessage("Resolving FCS999 address");

	m_addresses["FCS999"] = CUDPSocket::lookup("fcs999.xreflector.net");
	
	LogMessage("Opening FCS network connection");

	return m_socket.open();
}

void CFCSNetwork::clearDestination()
{
	m_pingTimer.stop();
	m_resetTimer.stop();

	m_state = FCS_UNLINKED;
}

void CFCSNetwork::write(const unsigned char* data)
{
	assert(data != NULL);

	if (m_state != FCS_LINKED)
		return;

	unsigned char buffer[130U];
	::memset(buffer + 0U, ' ', 130U);
	::memcpy(buffer + 0U, data + 35U, 120U);
	::memcpy(buffer + 120U, data + 34U, 1U);
	::memcpy(buffer + 121U, m_reflector.c_str(), 8U);

	if (m_debug)
		CUtils::dump(1U, "FCS Network Data Sent", buffer, 130U);

	m_socket.write(buffer, 130U, m_address, FCS_PORT);
}

bool CFCSNetwork::writeLink(const std::string& reflector)
{
	if (m_state != FCS_LINKED) {
		std::string name = reflector.substr(0U, 6U);
		
		if (m_addresses.count(name) == 0U) {			
			char fcs_url[30U];
			::sprintf(fcs_url, "%s.xreflector.net", name.c_str());
			m_address = CUDPSocket::lookup(fcs_url);

			if (m_address.s_addr == INADDR_NONE) {
				LogError("Unknown FCS reflector - %s", name.c_str());
				return false;
			}
    		} else {
			m_address = m_addresses[name];
   		}

		if (m_address.s_addr == INADDR_NONE) {
			LogError("FCS reflector %s has no address", name.c_str());
			return false;
		}
	}

	m_reflector = reflector;
	::memcpy(m_ping + 10U, reflector.c_str(), 8U);

	m_print = reflector.substr(0U, 6U) + "-" + reflector.substr(6U);

	m_state = FCS_LINKING;

	m_pingTimer.start();

	writePing();

	return true;
}

void CFCSNetwork::setOptions(const std::string& options)
{
	m_opt = options;
}

void CFCSNetwork::writeUnlink(unsigned int count)
{
	if (m_state != FCS_LINKED)
		return;

	for (unsigned int i = 0U; i < count; i++)
		m_socket.write((unsigned char*)"CLOSE      ", 11U, m_address, FCS_PORT);
}

void CFCSNetwork::clock(unsigned int ms)
{
	m_pingTimer.clock(ms);
	if (m_pingTimer.isRunning() && m_pingTimer.hasExpired()) {
		writePing();
		m_pingTimer.start();
	}

	m_resetTimer.clock(ms);
	if (m_resetTimer.isRunning() && m_resetTimer.hasExpired()) {
		m_n = 0U;
		m_resetTimer.stop();
	}

	unsigned char buffer[BUFFER_LENGTH];

	in_addr address;
	unsigned int port;
	int length = m_socket.read(buffer, BUFFER_LENGTH, address, port);
	if (length <= 0)
		return;

	if (m_state == FCS_UNLINKED)
		return;

	if (address.s_addr != m_address.s_addr || port != FCS_PORT)
		return;

	if (m_debug)
		CUtils::dump(1U, "FCS Network Data Received", buffer, length);

	if (length == 7) {
		if (m_state == FCS_LINKING)
			LogMessage("Linked to %s", m_print.c_str());
		m_state = FCS_LINKED;
		writeInfo();
		writeOptions(m_print);
	}

	if (length == 10 && m_state == FCS_LINKING) {
		LogMessage("Linked to %s", m_print.c_str());
		m_state = FCS_LINKED;
		writeInfo();
		writeOptions(m_print);
	}

	if (length == 7 || length == 10 || length == 130) {
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
	if (len != 130U) {
		m_buffer.getData(data, len);

		::memset(data + 0U, ' ', 14U);
		::memcpy(data + 0U, "YSFP", 4U);
		::memcpy(data + 4U, m_print.c_str(), 8U);

		return 14U;
	}

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
}

void CFCSNetwork::writeInfo()
{
	if (m_state != FCS_LINKED)
		return;

	if (m_debug)
		CUtils::dump(1U, "FCS Network Data Sent", m_info, 100U);

	m_socket.write(m_info, 100U, m_address, FCS_PORT);
}

void CFCSNetwork::writePing()
{
	if (m_state == FCS_UNLINKED)
		return;

	if (m_debug)
		CUtils::dump(1U, "FCS Network Data Sent", m_ping, 25U);

	m_socket.write(m_ping, 25U, m_address, FCS_PORT);
}

void CFCSNetwork::writeOptions(const std::string& reflector)
{
	if (m_state != FCS_LINKED)
		return;

	if (m_opt.size() < 1)
		return;

	::memset(m_options + 14U, 0x20U, 36U);
	::memcpy(m_options + 4U, (reflector.substr(0,6)+reflector.substr(7,2)).c_str(), 8U);
	::memcpy(m_options + 12U, m_opt.c_str(), m_opt.size());

	if (m_debug)
		CUtils::dump(1U, "FCS Network Options Sent", m_options, 50U);

	m_socket.write(m_options, 50U, m_address, FCS_PORT);
}
