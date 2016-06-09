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

#include <cstdio>
#include <cassert>
#include <cstring>

CReflectors::CReflectors(const std::string& hostsFile, unsigned int statusPort) :
m_hostsFile(hostsFile),
m_socket(statusPort),
m_reflectors(),
m_it(),
m_current(),
m_timer(1000U, 60U)
{
	assert(statusPort > 0U);
}

CReflectors::~CReflectors()
{
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

std::vector<CYSFReflector*>& CReflectors::current()
{
	m_current.clear();

	for (std::vector<CYSFReflector*>::iterator it = m_reflectors.begin(); it != m_reflectors.end(); ++it) {
		if ((*it)->m_timer.isRunning())
			m_current.push_back(*it);
	}

	return m_current;
}

void CReflectors::clock(unsigned int ms)
{
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

			LogDebug("Have YSFS reply from %s/%s/%s/%s", id.c_str(), name.c_str(), desc.c_str(), cnt.c_str());

			for (std::vector<CYSFReflector*>::iterator it = m_reflectors.begin(); it != m_reflectors.end(); ++it) {
				in_addr      itAddr = (*it)->m_address;
				unsigned int itPort = (*it)->m_port;

				if (itAddr.s_addr == address.s_addr && itPort == port) {
					(*it)->m_id    = id;
					(*it)->m_name  = name;
					(*it)->m_desc  = desc;
					(*it)->m_count = cnt;
					(*it)->m_timer.start();
					LogDebug("Updating %s", id.c_str());
					break;
				}
			}
		}
	}

	for (std::vector<CYSFReflector*>::iterator it = m_reflectors.begin(); it != m_reflectors.end(); ++it)
		(*it)->m_timer.clock(ms);
}
