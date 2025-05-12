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

#include <fstream>
#include <nlohmann/json.hpp>

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
	remove();
}

bool CYSFReflectors::load()
{
	remove();

	try {
		std::fstream file(m_hostsFile);

		nlohmann::json data = nlohmann::json::parse(file);

		bool hasData = data["reflectors"].is_array();
		if (!hasData)
			throw;

		nlohmann::json::array_t hosts = data["reflectors"];
		for (const auto& it : hosts) {
			std::string id = it["designator"];

			std::string country = it["country"];
			std::string name    = it["name"];
			bool useXX          = it["use_xx_prefix"];

			std::string fullName;
			if (useXX)
				fullName = "XX " + name;
			else
				fullName = country + " " + name;

			unsigned short port = it["port"];

			sockaddr_storage addr_v4 = sockaddr_storage();
			unsigned int     addrLen_v4 = 0U;

			bool isNull = it["ipv4"].is_null();
			if (!isNull) {
				std::string ipv4 = it["ipv4"];
				if (!CUDPSocket::lookup(ipv4, port, addr_v4, addrLen_v4) == 0) {
					LogWarning("Unable to resolve the address of %s", ipv4.c_str());
					addrLen_v4 = 0U;
				}
			}

			sockaddr_storage addr_v6 = sockaddr_storage();
			unsigned int     addrLen_v6 = 0U;

			isNull = it["ipv6"].is_null();
			if (!isNull) {
				std::string ipv6 = it["ipv6"];
				if (!CUDPSocket::lookup(ipv6, port, addr_v6, addrLen_v6) == 0) {
					LogWarning("Unable to resolve the address of %s", ipv6.c_str());
					addrLen_v6 = 0U;
				}
			}

			if ((addrLen_v4 > 0U) || (addrLen_v6 > 0U)) {
				CYSFReflector* refl = new CYSFReflector;
				refl->m_id           = id;
				refl->m_name         = fullName;

				refl->m_name.resize(16U, ' ');

				refl->IPv4.m_addr    = addr_v4;
				refl->IPv4.m_addrLen = addrLen_v4;
				refl->IPv6.m_addr    = addr_v6;
				refl->IPv6.m_addrLen = addrLen_v6;
				m_reflectors.push_back(refl);
			}
		}
	}
	catch (...) {
		LogError("Unable to load/parse JSON file %s", m_hostsFile.c_str());
		return false;
	}

	size_t size = m_reflectors.size();
	LogInfo("Loaded %u YSF reflectors", size);

	return true;
}

CYSFReflector* CYSFReflectors::findById(const std::string& id)
{
	for (const auto& it : m_reflectors) {
		if (id == it->m_id)
			return it;
	}

	LogMessage("Trying to find non existent YSF reflector with an id of %s", id.c_str());

	return nullptr;
}

CYSFReflector* CYSFReflectors::findByName(const std::string& name)
{
	for (const auto& it : m_reflectors) {
		if (name == it->m_name)
			return it;
	}

	LogMessage("Trying to find non existent YSF reflector with a name of %s", name.c_str());

	return nullptr;
}

void CYSFReflectors::remove()
{
	for (const auto& it : m_reflectors)
		delete it;

	m_reflectors.clear();
}
