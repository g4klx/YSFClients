/*
*   Copyright (C) 2016-2020,2025 by Jonathan Naylor G4KLX
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

#include "YSFReflectors.h"
#include "Log.h"

#include <algorithm>
#include <functional>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cctype>

CYSFReflectors::CYSFReflectors(const std::string& hostsFile) :
m_hostsFile(hostsFile),
m_reflectors()
{
}

CYSFReflectors::~CYSFReflectors()
{
	for (std::vector<CYSFReflector*>::iterator it = m_reflectors.begin(); it != m_reflectors.end(); ++it)
		delete *it;

	m_reflectors.clear();
}

bool CYSFReflectors::load()
{
	FILE* fp = ::fopen(m_hostsFile.c_str(), "rt");
	if (fp != nullptr) {
		char buffer[100U];
		while (::fgets(buffer, 100U, fp) != nullptr) {
			if (buffer[0U] == '#')
				continue;

			char* p1 = ::strtok(buffer, ";\r\n");
			char* p2 = ::strtok(nullptr, ";\r\n");
			char* p3 = ::strtok(nullptr, ";\r\n");
			char* p4 = ::strtok(nullptr, ";\r\n");
			char* p5 = ::strtok(nullptr, ";\r\n");
			char* p6 = ::strtok(nullptr, "\r\n");

			if (p1 != nullptr && p2 != nullptr && p3 != nullptr && p4 != nullptr && p5 != nullptr && p6 != nullptr) {
				std::string host = std::string(p4);
				unsigned short port = (unsigned short)::atoi(p5);

				if (::strstr(p1, "YCS") == nullptr && ::strstr(p2, "YCS") == nullptr) {
					sockaddr_storage addr;
					unsigned int addrLen;
					if (CUDPSocket::lookup(host, port, addr, addrLen) == 0) {
						CYSFReflector* refl = new CYSFReflector;
						refl->m_id = std::string(p1);
						refl->m_name = std::string(p2);
						refl->m_addr = addr;
						refl->m_addrLen = addrLen;
						m_reflectors.push_back(refl);
					} else {
						LogWarning("Unable to resolve the address for %s", host.c_str());
					}
				}
			}
		}

		::fclose(fp);
	}

	size_t size = m_reflectors.size();
	LogInfo("Loaded %u YSF reflectors", size);

	return true;
}

CYSFReflector* CYSFReflectors::findById(const std::string& id)
{
	for (std::vector<CYSFReflector*>::const_iterator it = m_reflectors.cbegin(); it != m_reflectors.cend(); ++it) {
		if (id == (*it)->m_id)
			return *it;
	}

	LogMessage("Trying to find non existent YSF reflector with an id of %s", id.c_str());

	return nullptr;
}

CYSFReflector* CYSFReflectors::findByName(const std::string& name)
{
	for (std::vector<CYSFReflector*>::const_iterator it = m_reflectors.cbegin(); it != m_reflectors.cend(); ++it) {
		if (name == (*it)->m_name)
			return *it;
	}

	LogMessage("Trying to find non existent YSF reflector with a name of %s", name.c_str());

	return nullptr;
}

