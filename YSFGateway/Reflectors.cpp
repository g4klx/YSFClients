/*
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
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

#include "Reflectors.h"
#include "Hosts.h"
#include "Log.h"

#include <algorithm>
#include <functional>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cctype>

CReflectors::CReflectors(const std::string& hostsFile, unsigned int statusPort) :
m_hostsFile(hostsFile),
m_socket(statusPort),
m_reflectors(),
m_it(),
m_current(),
m_search(),
m_timer(1000U, 30U)
{
	assert(statusPort > 0U);
}

CReflectors::~CReflectors()
{
	for (std::vector<CYSFReflector*>::iterator it = m_reflectors.begin(); it != m_reflectors.end(); ++it)
		delete *it;
}

bool CReflectors::load()
{
	bool ret = m_socket.open();
	if (!ret)
		return false;

	CHosts hosts(m_hostsFile);
	ret = hosts.read();
	if (!ret)
		return false;

	std::vector<CYSFHost*>& hostList = hosts.list();

	for (std::vector<CYSFHost*>::const_iterator it = hostList.begin(); it != hostList.end(); ++it) {
		in_addr address = CUDPSocket::lookup((*it)->m_address);

		if (address.s_addr != INADDR_NONE) {
			CYSFReflector* reflector = new CYSFReflector;
			reflector->m_address = address;
			reflector->m_port    = (*it)->m_port;
			m_reflectors.push_back(reflector);
		}
	}

	m_it = m_reflectors.begin();

	// Make the polling time based on the number of reflectors within limits
	unsigned int nReflectors = m_reflectors.size();
	if (nReflectors > 0U) {
		unsigned int t = 600U / nReflectors;
		if (t > 30U)
			t = 30U;
		else if (t < 15U)
			t = 15U;

		m_timer.setTimeout(t);
	}

	m_timer.start();

	return true;
}

CYSFReflector* CReflectors::find(const std::string& id)
{
	for (std::vector<CYSFReflector*>::iterator it = m_reflectors.begin(); it != m_reflectors.end(); ++it) {
		if (id == (*it)->m_id)
			return *it;
	}

	LogMessage("Trying to find non existent reflector with an id of %s", id.c_str());

	return NULL;
}

static bool refComparison(const CYSFReflector* r1, const CYSFReflector* r2)
{
	assert(r1 != NULL);
	assert(r2 != NULL);

	std::string name1 = r1->m_name;
	std::string name2 = r2->m_name;

	for (unsigned int i = 0U; i < 16U; i++) {
		int c = ::toupper(name1.at(i)) - ::toupper(name2.at(i));
		if (c != 0)
			return c < 0;
	}

	return false;
}

std::vector<CYSFReflector*>& CReflectors::current()
{
	m_current.clear();

	for (std::vector<CYSFReflector*>::iterator it = m_reflectors.begin(); it != m_reflectors.end(); ++it) {
		if ((*it)->m_seen)
			m_current.push_back(*it);
	}

	std::sort(m_current.begin(), m_current.end(), refComparison);

	return m_current;
}

std::vector<CYSFReflector*>& CReflectors::search(const std::string& name)
{
	m_search.clear();

	std::string trimmed = name;
	trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), trimmed.end());
	std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(), ::toupper);

	unsigned int len = trimmed.size();

	for (std::vector<CYSFReflector*>::iterator it = m_reflectors.begin(); it != m_reflectors.end(); ++it) {
		if (!(*it)->m_seen)
			continue;

		std::string reflector = (*it)->m_name;
		reflector.erase(std::find_if(reflector.rbegin(), reflector.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), reflector.end());
		std::transform(reflector.begin(), reflector.end(), reflector.begin(), ::toupper);

		if (trimmed == reflector.substr(0U, len))
			m_search.push_back(*it);
	}

	std::sort(m_search.begin(), m_search.end(), refComparison);

	return m_search;
}

void CReflectors::clock(unsigned int ms)
{
	// Nothing to do, avoid crashes
	if (m_reflectors.size() == 0U)
		return;

	m_timer.clock(ms);
	if (m_timer.isRunning() && m_timer.hasExpired()) {
		m_socket.write((unsigned char*)"YSFS", 4U, (*m_it)->m_address, (*m_it)->m_port);

		++m_it;
		if (m_it == m_reflectors.end())
			m_it = m_reflectors.begin();

		m_timer.start();
	}

	in_addr address;
	unsigned int port;
	unsigned char buffer[200U];
	int ret = m_socket.read(buffer, 200U, address, port);

	if (ret > 0) {
		if (::memcmp(buffer + 0U, "YSFS", 4U) == 0) {
			buffer[42U] = 0x00U;

			std::string id   = std::string((char*)(buffer + 4U), 5U);
			std::string name = std::string((char*)(buffer + 9U), 16U);
			std::string desc = std::string((char*)(buffer + 25U), 14U);
			std::string cnt  = std::string((char*)(buffer + 39U), 3U);

			LogDebug("Have reflector status reply from %s/%s/%s/%s", id.c_str(), name.c_str(), desc.c_str(), cnt.c_str());

			for (std::vector<CYSFReflector*>::iterator it = m_reflectors.begin(); it != m_reflectors.end(); ++it) {
				in_addr      itAddr = (*it)->m_address;
				unsigned int itPort = (*it)->m_port;

				if (itAddr.s_addr == address.s_addr && itPort == port) {
					(*it)->m_id    = id;
					(*it)->m_name  = name;
					(*it)->m_desc  = desc;
					(*it)->m_count = cnt;
					(*it)->m_seen  = true;
					break;
				}
			}
		}
	}
}
