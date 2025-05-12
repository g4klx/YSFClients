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

CYSFReflectors::CYSFReflectors(const std::string& hostsFile, unsigned int reloadTime, bool makeUpper) :
m_hostsFile(hostsFile),
m_parrotAddress(),
m_parrotPort(0U),
m_YSF2DMRAddress(),
m_YSF2DMRPort(0U),
m_YSF2NXDNAddress(),
m_YSF2NXDNPort(0U),
m_YSF2P25Address(),
m_YSF2P25Port(0U),
m_fcsRooms(),
m_newReflectors(),
m_currReflectors(),
m_search(),
m_makeUpper(makeUpper),
m_timer(1000U, reloadTime * 60U) 
{
	if (reloadTime > 0U)
		m_timer.start();
}

CYSFReflectors::~CYSFReflectors()
{
	for (const auto& it : m_newReflectors)
		delete it;

	for (const auto& it : m_currReflectors)
		delete it;

	m_newReflectors.clear();
	m_currReflectors.clear();
}

static bool refComparison(const CYSFReflector* r1, const CYSFReflector* r2)
{
	assert(r1 != nullptr);
	assert(r2 != nullptr);

	std::string name1 = r1->m_name;
	std::string name2 = r2->m_name;

	for (unsigned int i = 0U; i < 16U; i++) {
		int c = ::toupper(name1.at(i)) - ::toupper(name2.at(i));
		if (c != 0)
			return c < 0;
	}

	return false;
}

void CYSFReflectors::setParrot(const std::string& address, unsigned short port)
{
	m_parrotAddress = address;
	m_parrotPort    = port;
}

void CYSFReflectors::setYSF2DMR(const std::string& address, unsigned short port)
{
	m_YSF2DMRAddress = address;
	m_YSF2DMRPort    = port;
}

void CYSFReflectors::setYSF2NXDN(const std::string& address, unsigned short port)
{
	m_YSF2NXDNAddress = address;
	m_YSF2NXDNPort    = port;
}

void CYSFReflectors::setYSF2P25(const std::string& address, unsigned short port)
{
	m_YSF2P25Address = address;
	m_YSF2P25Port    = port;
}

void CYSFReflectors::addFCSRoom(const std::string& id, const std::string& name)
{
	m_fcsRooms.push_back(std::make_pair(id, name));
}

bool CYSFReflectors::load()
{
	for (const auto& it : m_newReflectors)
		delete it;

	m_newReflectors.clear();

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

			std::string desc = it["description"];

			LogMessage("Id: %s, name: %s, desc: %s", id.c_str(), fullName.c_str(), desc.c_str());

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
				refl->m_desc         = desc;
				refl->m_count        = "000";
				refl->m_type         = YSF_TYPE::YSF;
				refl->m_wiresX       = (name.compare(0, 3, "XLX") == 0);

				refl->m_name.resize(16U, ' ');
				refl->m_desc.resize(14U, ' ');

				refl->IPv4.m_addr    = addr_v4;
				refl->IPv4.m_addrLen = addrLen_v4;
				refl->IPv6.m_addr    = addr_v6;
				refl->IPv6.m_addrLen = addrLen_v6;
				m_newReflectors.push_back(refl);
			}
		}
	}
	catch (...) {
		LogError("Unable to load/parse JSON file %s", m_hostsFile.c_str());
		return false;
	}

	size_t size = m_newReflectors.size();
	LogInfo("Loaded %u YSF reflectors", size);

	// Add the Parrot entry
	if (m_parrotPort > 0U) {
		sockaddr_storage addr;
		unsigned int addrLen;
		if (CUDPSocket::lookup(m_parrotAddress, m_parrotPort, addr, addrLen) == 0) {
			CYSFReflector* refl = new CYSFReflector;
			refl->m_id      = "00001";
			refl->m_name    = "ZZ Parrot       ";
			refl->m_desc    = "Parrot        ";
			switch (addr.ss_family) {
			case AF_INET:
				refl->IPv4.m_addr    = addr;
				refl->IPv4.m_addrLen = addrLen;
				refl->IPv6.m_addrLen = 0U;
				break;
			case AF_INET6:
				refl->IPv6.m_addr    = addr;
				refl->IPv6.m_addrLen = addrLen;
				refl->IPv4.m_addrLen = 0U;
				break;
			default:
				refl->IPv6.m_addrLen = 0U;
				refl->IPv4.m_addrLen = 0U;
				LogWarning("Unknown address family for %s", m_parrotAddress.c_str());
				break;
			}
			refl->m_count   = "000";
			refl->m_type    = YSF_TYPE::YSF;
			refl->m_wiresX  = false;

			m_newReflectors.push_back(refl);

			LogInfo("Loaded YSF parrot");
		} else {
			LogWarning("Unable to resolve the address of the YSF Parrot");
		}
	}

	// Add the YSF2DMR entry
	if (m_YSF2DMRPort > 0U) {
		sockaddr_storage addr;
		unsigned int addrLen;
		if (CUDPSocket::lookup(m_YSF2DMRAddress, m_YSF2DMRPort, addr, addrLen) == 0) {
			CYSFReflector* refl = new CYSFReflector;
			refl->m_id      = "00002";
			refl->m_name    = "YSF2DMR         ";
			refl->m_desc    = "Link YSF2DMR  ";
			switch (addr.ss_family) {
			case AF_INET:
				refl->IPv4.m_addr = addr;
				refl->IPv4.m_addrLen = addrLen;
				refl->IPv6.m_addrLen = 0U;
				break;
			case AF_INET6:
				refl->IPv6.m_addr = addr;
				refl->IPv6.m_addrLen = addrLen;
				refl->IPv4.m_addrLen = 0U;
				break;
			default:
				refl->IPv6.m_addrLen = 0U;
				refl->IPv4.m_addrLen = 0U;
				LogWarning("Unknown address family for %s", m_parrotAddress.c_str());
				break;
			}
			refl->m_count   = "000";
			refl->m_type    = YSF_TYPE::YSF;
			refl->m_wiresX  = true;

			m_newReflectors.push_back(refl);

			LogInfo("Loaded YSF2DMR");
		} else {
			LogWarning("Unable to resolve the address of YSF2DMR");
		}
	}

	// Add the YSF2NXDN entry
	if (m_YSF2NXDNPort > 0U) {
		sockaddr_storage addr;
		unsigned int addrLen;
		if (CUDPSocket::lookup(m_YSF2NXDNAddress, m_YSF2NXDNPort, addr, addrLen) == 0) {
			CYSFReflector* refl = new CYSFReflector;
			refl->m_id      = "00003";
			refl->m_name    = "YSF2NXDN        ";
			refl->m_desc    = "Link YSF2NXDN ";
			switch (addr.ss_family) {
			case AF_INET:
				refl->IPv4.m_addr = addr;
				refl->IPv4.m_addrLen = addrLen;
				refl->IPv6.m_addrLen = 0U;
				break;
			case AF_INET6:
				refl->IPv6.m_addr = addr;
				refl->IPv6.m_addrLen = addrLen;
				refl->IPv4.m_addrLen = 0U;
				break;
			default:
				refl->IPv6.m_addrLen = 0U;
				refl->IPv4.m_addrLen = 0U;
				LogWarning("Unknown address family for %s", m_parrotAddress.c_str());
				break;
			}
			refl->m_count   = "000";
			refl->m_type    = YSF_TYPE::YSF;
			refl->m_wiresX  = true;

			m_newReflectors.push_back(refl);

			LogInfo("Loaded YSF2NXDN");
		} else {
			LogWarning("Unable to resolve the address of YSF2NXDN");
		}
	}

	// Add the YSF2P25 entry
	if (m_YSF2P25Port > 0U) {
		sockaddr_storage addr;
		unsigned int addrLen;
		if (CUDPSocket::lookup(m_YSF2P25Address, m_YSF2P25Port, addr, addrLen) == 0) {
			CYSFReflector* refl = new CYSFReflector;
			refl->m_id      = "00004";
			refl->m_name    = "YSF2P25         ";
			refl->m_desc    = "Link YSF2P25  ";
			switch (addr.ss_family) {
			case AF_INET:
				refl->IPv4.m_addr = addr;
				refl->IPv4.m_addrLen = addrLen;
				refl->IPv6.m_addrLen = 0U;
				break;
			case AF_INET6:
				refl->IPv6.m_addr = addr;
				refl->IPv6.m_addrLen = addrLen;
				refl->IPv4.m_addrLen = 0U;
				break;
			default:
				refl->IPv6.m_addrLen = 0U;
				refl->IPv4.m_addrLen = 0U;
				LogWarning("Unknown address family for %s", m_parrotAddress.c_str());
				break;
			}
			refl->m_count   = "000";
			refl->m_type    = YSF_TYPE::YSF;
			refl->m_wiresX  = true;

			m_newReflectors.push_back(refl);

			LogInfo("Loaded YSF2P25");
		} else {
			LogWarning("Unable to resolve the address of YSF2P25");
		}
	}

	unsigned int id = 9U;
	for (const auto& it1 : m_fcsRooms) {
		bool used;
		do {
			id++;
			used = findById(id);
		} while (used);

		char text[10U];
		::sprintf(text, "%05u", id);

		std::string name = it1.first;
		std::string desc = it1.second;

		CYSFReflector* refl = new CYSFReflector;
		refl->m_id           = text;
		refl->m_name         = name;
		refl->m_desc         = desc;
		refl->IPv4.m_addrLen = 0U;
		refl->IPv6.m_addrLen = 0U;
		refl->m_count        = "000";
		refl->m_type         = YSF_TYPE::FCS;
		refl->m_wiresX       = false;

		refl->m_name.resize(16U, ' ');
		refl->m_desc.resize(14U, ' ');

		m_newReflectors.push_back(refl);
	}

	size = m_newReflectors.size();
	if (size == 0U)
		return false;

	if (m_makeUpper) {
		for (auto& it : m_newReflectors) {
			std::transform(it->m_name.begin(), it->m_name.end(), it->m_name.begin(), ::toupper);
			std::transform(it->m_desc.begin(), it->m_desc.end(), it->m_desc.begin(), ::toupper);
		}
	}

	std::sort(m_newReflectors.begin(), m_newReflectors.end(), refComparison);

	return true;
}

CYSFReflector* CYSFReflectors::findById(const std::string& id)
{
	for (const auto& it : m_currReflectors) {
		if (id == it->m_id)
			return it;
	}

	LogMessage("Trying to find non existent YSF reflector with an id of %s", id.c_str());

	return nullptr;
}

bool CYSFReflectors::findById(unsigned int id) const
{
	char text[10U];
	::sprintf(text, "%05u", id);

	for (const auto& it : m_newReflectors) {
		if (text == it->m_id)
			return true;
	}

	return false;
}

CYSFReflector* CYSFReflectors::findByName(const std::string& name)
{
	std::string fullName = name;
	if (m_makeUpper)
		std::transform(fullName.begin(), fullName.end(), fullName.begin(), ::toupper);
	fullName.resize(16U, ' ');

	for (std::vector<CYSFReflector*>::const_iterator it = m_currReflectors.cbegin(); it != m_currReflectors.cend(); ++it) {
		if (fullName == (*it)->m_name)
			return *it;
	}

	LogMessage("Trying to find non existent YSF reflector with a name of %s", name.c_str());

	return nullptr;
}

std::vector<CYSFReflector*>& CYSFReflectors::current()
{
	return m_currReflectors;
}

std::vector<CYSFReflector*>& CYSFReflectors::search(const std::string& name)
{
	m_search.clear();

	std::string trimmed = name;
	trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), trimmed.end());
	std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(), ::toupper);

	// Removed now un-used variable
	// size_t len = trimmed.size();

	for (auto& it : m_currReflectors) {
		std::string reflector = it->m_name;
		reflector.erase(std::find_if(reflector.rbegin(), reflector.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), reflector.end());
		std::transform(reflector.begin(), reflector.end(), reflector.begin(), ::toupper);

		// Original match function - only matches start of string.
		// if (trimmed == reflector.substr(0U, len))
		// 	m_search.push_back(*it);

		// New match function searches the whole string
		for (unsigned int refSrcPos = 0U; refSrcPos < reflector.length(); refSrcPos++) {
			if (reflector.substr(refSrcPos, trimmed.length()) == trimmed) {
				m_search.push_back(it);
				break;
			}
		}
	}

	std::sort(m_search.begin(), m_search.end(), refComparison);

	return m_search;
}

bool CYSFReflectors::reload()
{
	if (m_newReflectors.empty())
		return false;

	for (const auto& it : m_currReflectors)
		delete it;

	m_currReflectors.clear();

	m_currReflectors = m_newReflectors;

	m_newReflectors.clear();

	return true;
}

void CYSFReflectors::clock(unsigned int ms)
{
	m_timer.clock(ms);

	if (m_timer.isRunning() && m_timer.hasExpired()) {
		load();
		m_timer.start();
	}
}
