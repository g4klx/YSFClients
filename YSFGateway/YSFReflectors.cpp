/*
*   Copyright (C) 2016-2019 by Jonathan Naylor G4KLX
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
	for (std::vector<CYSFReflector*>::iterator it = m_newReflectors.begin(); it != m_newReflectors.end(); ++it)
		delete *it;

	for (std::vector<CYSFReflector*>::iterator it = m_currReflectors.begin(); it != m_currReflectors.end(); ++it)
		delete *it;

	m_newReflectors.clear();
	m_currReflectors.clear();
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

void CYSFReflectors::setParrot(const std::string& address, unsigned int port)
{
	m_parrotAddress = address;
	m_parrotPort    = port;
}

void CYSFReflectors::setYSF2DMR(const std::string& address, unsigned int port)
{
	m_YSF2DMRAddress = address;
	m_YSF2DMRPort    = port;
}

void CYSFReflectors::setYSF2NXDN(const std::string& address, unsigned int port)
{
	m_YSF2NXDNAddress = address;
	m_YSF2NXDNPort    = port;
}

void CYSFReflectors::setYSF2P25(const std::string& address, unsigned int port)
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
	for (std::vector<CYSFReflector*>::iterator it = m_newReflectors.begin(); it != m_newReflectors.end(); ++it)
		delete *it;

	m_newReflectors.clear();

	FILE* fp = ::fopen(m_hostsFile.c_str(), "rt");
	if (fp != NULL) {
		char buffer[100U];
		while (::fgets(buffer, 100U, fp) != NULL) {
			if (buffer[0U] == '#')
				continue;

			char* p1 = ::strtok(buffer, ";\r\n");
			char* p2 = ::strtok(NULL, ";\r\n");
			char* p3 = ::strtok(NULL, ";\r\n");
			char* p4 = ::strtok(NULL, ";\r\n");
			char* p5 = ::strtok(NULL, ";\r\n");
			char* p6 = ::strtok(NULL, "\r\n");

			if (p1 != NULL && p2 != NULL && p3 != NULL && p4 != NULL && p5 != NULL && p6 != NULL) {
				std::string host = std::string(p4);

				in_addr address = CUDPSocket::lookup(host);
				if (address.s_addr != INADDR_NONE) {
					CYSFReflector* refl = new CYSFReflector;
					refl->m_id      = std::string(p1);
					refl->m_name    = std::string(p2);
					refl->m_desc    = std::string(p3);
					refl->m_address = address;
					refl->m_port    = (unsigned int)::atoi(p5);
					refl->m_count   = std::string(p6);
					refl->m_type    = YT_YSF;
					refl->m_wiresX  = false;

					refl->m_name.resize(16U, ' ');
					refl->m_desc.resize(14U, ' ');

					m_newReflectors.push_back(refl);
				}
			}
		}

		::fclose(fp);
	}

	size_t size = m_newReflectors.size();
	LogInfo("Loaded %u YSF reflectors", size);

	// Add the Parrot entry
	if (m_parrotPort > 0U) {
		CYSFReflector* refl = new CYSFReflector;
		refl->m_id      = "00001";
		refl->m_name    = "ZZ Parrot       ";
		refl->m_desc    = "Parrot        ";
		refl->m_address = CUDPSocket::lookup(m_parrotAddress);
		refl->m_port    = m_parrotPort;
		refl->m_count   = "000";
		refl->m_type    = YT_YSF;
		refl->m_wiresX  = false;

		m_newReflectors.push_back(refl);

		LogInfo("Loaded YSF parrot");
	}

	// Add the YSF2DMR entry
	if (m_YSF2DMRPort > 0U) {
		CYSFReflector* refl = new CYSFReflector;
		refl->m_id      = "00002";
		refl->m_name    = "YSF2DMR         ";
		refl->m_desc    = "Link YSF2DMR  ";
		refl->m_address = CUDPSocket::lookup(m_YSF2DMRAddress);
		refl->m_port    = m_YSF2DMRPort;
		refl->m_count   = "000";
		refl->m_type    = YT_YSF;
		refl->m_wiresX  = true;

		m_newReflectors.push_back(refl);

		LogInfo("Loaded YSF2DMR");
	}

	// Add the YSF2NXDN entry
	if (m_YSF2NXDNPort > 0U) {
		CYSFReflector* refl = new CYSFReflector;
		refl->m_id      = "00003";
		refl->m_name    = "YSF2NXDN        ";
		refl->m_desc    = "Link YSF2NXDN ";
		refl->m_address = CUDPSocket::lookup(m_YSF2NXDNAddress);
		refl->m_port    = m_YSF2NXDNPort;
		refl->m_count   = "000";
		refl->m_type    = YT_YSF;
		refl->m_wiresX  = true;

		m_newReflectors.push_back(refl);

		LogInfo("Loaded YSF2NXDN");
	}

	// Add the YSF2P25 entry
	if (m_YSF2P25Port > 0U) {
		CYSFReflector* refl = new CYSFReflector;
		refl->m_id      = "00004";
		refl->m_name    = "YSF2P25         ";
		refl->m_desc    = "Link YSF2P25  ";
		refl->m_address = CUDPSocket::lookup(m_YSF2P25Address);
		refl->m_port    = m_YSF2P25Port;
		refl->m_count   = "000";
		refl->m_type    = YT_YSF;
		refl->m_wiresX  = true;

		m_newReflectors.push_back(refl);

		LogInfo("Loaded YSF2P25");
	}

	unsigned int id = 10U;
	for (std::vector<std::pair<std::string, std::string>>::const_iterator it = m_fcsRooms.cbegin(); it != m_fcsRooms.cend(); ++it, id++) {
		char text[10U];
		::sprintf(text, "%05u", id);

		std::string name = it->first;
		std::string desc = it->second;

		CYSFReflector* refl = new CYSFReflector;
		refl->m_id     = text;
		refl->m_name   = name;
		refl->m_desc   = desc;
		refl->m_port   = 0U;
		refl->m_count  = "000";
		refl->m_type   = YT_FCS;
		refl->m_wiresX = false;

		refl->m_name.resize(16U, ' ');
		refl->m_desc.resize(14U, ' ');

		m_newReflectors.push_back(refl);
	}

	size = m_newReflectors.size();
	if (size == 0U)
		return false;

	if (m_makeUpper) {
		for (std::vector<CYSFReflector*>::iterator it = m_newReflectors.begin(); it != m_newReflectors.end(); ++it) {
			std::transform((*it)->m_name.begin(), (*it)->m_name.end(), (*it)->m_name.begin(), ::toupper);
			std::transform((*it)->m_desc.begin(), (*it)->m_desc.end(), (*it)->m_desc.begin(), ::toupper);
		}
	}

	std::sort(m_newReflectors.begin(), m_newReflectors.end(), refComparison);

	return true;
}

CYSFReflector* CYSFReflectors::findById(const std::string& id)
{
	for (std::vector<CYSFReflector*>::const_iterator it = m_currReflectors.cbegin(); it != m_currReflectors.cend(); ++it) {
		if (id == (*it)->m_id)
			return *it;
	}

	LogMessage("Trying to find non existent YSF reflector with an id of %s", id.c_str());

	return NULL;
}

CYSFReflector* CYSFReflectors::findByName(const std::string& name)
{
	std::string fullName = name;
	if (m_makeUpper) {
                std::transform(fullName.begin(), fullName.end(), fullName.begin(), ::toupper);
        }
	fullName.resize(16U, ' ');

	for (std::vector<CYSFReflector*>::const_iterator it = m_currReflectors.cbegin(); it != m_currReflectors.cend(); ++it) {
		if (fullName == (*it)->m_name)
			return *it;
	}

	LogMessage("Trying to find non existent YSF reflector with a name of %s", name.c_str());

	return NULL;
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

	for (std::vector<CYSFReflector*>::iterator it = m_currReflectors.begin(); it != m_currReflectors.end(); ++it) {
		std::string reflector = (*it)->m_name;
		reflector.erase(std::find_if(reflector.rbegin(), reflector.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), reflector.end());
		std::transform(reflector.begin(), reflector.end(), reflector.begin(), ::toupper);

		// Original match function - only matches start of string.
		// if (trimmed == reflector.substr(0U, len))
		// 	m_search.push_back(*it);
		
		// New match function searches the whole string
		for (unsigned int refSrcPos = 0U; refSrcPos < reflector.length(); refSrcPos++) {
			if (reflector.substr(refSrcPos, trimmed.length()) == trimmed) {
				m_search.push_back(*it);
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

	for (std::vector<CYSFReflector*>::iterator it = m_currReflectors.begin(); it != m_currReflectors.end(); ++it)
		delete *it;

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
