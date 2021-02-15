/*
 *   Copyright (C) 2021 by Jonathan Naylor G4KLX
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

#include "BlockList.h"
#include "YSFDefines.h"
#include "Log.h"

#include <algorithm>

#include <cstdio>
#include <cassert>
#include <cstring>

const int BUFFER_SIZE = 500;

CBlockList::CBlockList(const std::string& file, unsigned int time) :
m_file(file),
m_time(time),
m_callsigns(),
m_timer(1000U, time * 60U),
m_checksum(0U)
{
}

CBlockList::~CBlockList()
{
}

bool CBlockList::start()
{
	loadFile();

	m_timer.start();

	return true;
}

bool CBlockList::check(const unsigned char* callsign) const
{
	assert(callsign != NULL);

	if (m_callsigns.empty())
		return false;

	std::string call = std::string((char*)callsign, YSF_CALLSIGN_LENGTH);
	std::for_each(call.begin(), call.end(), [](char& c) {
		c = std::toupper(c);
	});

	for (std::vector<std::string>::const_iterator it = m_callsigns.cbegin(); it != m_callsigns.cend(); ++it) {
		const std::string blocked = *it;
		if (call.find(blocked) != std::string::npos)
			return true;
	}

	return false;
}

void CBlockList::clock(unsigned int ms)
{
	m_timer.clock(ms);

	if (m_timer.isRunning() && m_timer.hasExpired()) {
		loadFile();
		m_timer.start();
	}
}

bool CBlockList::loadFile()
{
	FILE* fp = ::fopen(m_file.c_str(), "rt");
	if (fp == NULL) {
		if (!m_callsigns.empty()) {
			m_callsigns.clear();
			LogInfo("Loaded %u callsigns from the block list", m_callsigns.size());
		}
		return false;
	}

	char buffer[BUFFER_SIZE];

	// First perform the checksum (Fletcher 16)
	uint16_t sum1 = 0U;
	uint16_t sum2 = 0U;
	while (::fgets(buffer, BUFFER_SIZE, fp) != NULL) {
		for (char* p = buffer; *p != '\0'; p++) {
			sum1 = (sum1 + uint8_t(*p)) % 255U;
			sum2 = (sum2 + sum1) % 255U;
		}
	}

	uint16_t checksum = (sum2 << 8) | sum1;
	if (checksum == m_checksum) {
		::fclose(fp);
		return false;
	}

	m_checksum = checksum;

	// Rewind the file
	::fseek(fp, 0L, SEEK_SET);

	// Read the callsigns into the array
	m_callsigns.clear();

	while (::fgets(buffer, BUFFER_SIZE, fp) != NULL) {
		char* p;
		if ((p = ::strchr(buffer, '\n')) != NULL)
			*p = '\0';
		if ((p = ::strchr(buffer, '\r')) != NULL)
			*p = '\0';

		if (::strlen(buffer) > 0U) {
			std::string callsign = std::string(buffer);

			std::for_each(callsign.begin(), callsign.end(), [](char& c) {
				c = std::toupper(c);
			});

			m_callsigns.push_back(callsign);
		}
	}

	::fclose(fp);

	LogInfo("Loaded %u callsigns from the block list", m_callsigns.size());

	return true;
}
