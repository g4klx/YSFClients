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

#include "Hosts.h"
#include "Log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

CHosts::CHosts(const std::string& filename) :
m_filename(filename),
m_hosts()
{
}

CHosts::~CHosts()
{
}

bool CHosts::read()
{
	FILE* fp = ::fopen(m_filename.c_str(), "rt");
	if (fp == NULL) {
		LogWarning("Cannot open the YSF Hosts file - %s", m_filename.c_str());
		return false;
	}

	char buffer[100U];
	while (::fgets(buffer, 100U, fp) != NULL) {
		if (buffer[0U] == '#')
			continue;

		char* p1 = ::strtok(buffer, " \t\r\n");
		char* p2 = ::strtok(NULL, " \t\r\n");

		if (p1 != NULL && p2 != NULL) {
			std::string address = std::string(p1);
			unsigned int port   = (unsigned int)::atoi(p2);

			CYSFHost* host = new CYSFHost;
			host->m_address = address;
			host->m_port    = port;

			m_hosts.push_back(host);
		}
	}

	::fclose(fp);

	size_t size = m_hosts.size();
	if (size == 0U)
		return false;

	LogInfo("Loaded %u YSF reflectors", size);

	return true;
}

std::vector<CYSFHost*>& CHosts::list()
{
	return m_hosts;
}
