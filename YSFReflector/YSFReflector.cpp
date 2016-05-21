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

#include "YSFReflector.h"
#include "StopWatch.h"
#include "Network.h"
#include "Version.h"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv)
{
	if (argc == 1) {
		::fprintf(stderr, "Usage: YSFReflector <port>\n");
		return 1;
	}

	unsigned int port = ::atoi(argv[1]);
	if (port == 0U) {
		::fprintf(stderr, "YSFReflector: invalid port number\n");
		return 1;
	}

	CYSFReflector Reflector(port);
	Reflector.run();

	return 0;
}

CYSFReflector::CYSFReflector(unsigned int port) :
m_port(port),
m_repeaters()
{
}

CYSFReflector::~CYSFReflector()
{
}

void CYSFReflector::run()
{
	CNetwork network(m_port, false);

	bool ret = network.open();
	if (!ret)
		return;

	CStopWatch stopWatch;
	stopWatch.start();

	CTimer pollTimer(1000U, 5U);
	pollTimer.start();

	::fprintf(stdout, "Starting YSFReflector-%s\n", VERSION);

	CTimer watchdogTimer(1000U, 0U, 1500U);

	unsigned char src[YSF_CALLSIGN_LENGTH];
	unsigned char dst[YSF_CALLSIGN_LENGTH];

	for (;;) {
		unsigned char buffer[200U];

		unsigned int len = network.readData(buffer);
		if (len > 0U) {
			if (!watchdogTimer.isRunning()) {
				if (::memcmp(buffer + 14U, "          ", YSF_CALLSIGN_LENGTH) != 0)
					::memcpy(src, buffer + 14U, YSF_CALLSIGN_LENGTH);
				else
					::memcpy(src, "??????????", YSF_CALLSIGN_LENGTH);

				if (::memcmp(buffer + 24U, "          ", YSF_CALLSIGN_LENGTH) != 0)
					::memcpy(dst, buffer + 24U, YSF_CALLSIGN_LENGTH);
				else
					::memcpy(dst, "??????????", YSF_CALLSIGN_LENGTH);

				::fprintf(stdout, "Received data from %10.10s to %10.10s at %10.10s\n", src, dst, buffer + 4U);
			}
			else {
				bool changed = false;

				if (::memcmp(buffer + 14U, "          ", YSF_CALLSIGN_LENGTH) != 0 && ::memcmp(src, "??????????", YSF_CALLSIGN_LENGTH) == 0) {
					::memcpy(src, buffer + 14U, YSF_CALLSIGN_LENGTH);
					changed = true;
				}

				if (::memcmp(buffer + 24U, "          ", YSF_CALLSIGN_LENGTH) != 0 && ::memcmp(dst, "??????????", YSF_CALLSIGN_LENGTH) == 0) {
					::memcpy(dst, buffer + 24U, YSF_CALLSIGN_LENGTH);
					changed = true;
				}

				if (changed)
					::fprintf(stdout, "Received data from %10.10s to %10.10s at %10.10s\n", src, dst, buffer + 4U);
			}

			watchdogTimer.start();

			std::string callsign = std::string((char*)(buffer + 4U), YSF_CALLSIGN_LENGTH);
			CYSFRepeater* rpt = findRepeater(callsign);
			if (rpt != NULL) {
				for (std::vector<CYSFRepeater*>::const_iterator it = m_repeaters.begin(); it != m_repeaters.end(); ++it) {
					if ((*it)->m_callsign != callsign)
						network.writeData(buffer, (*it)->m_address, (*it)->m_port);
				}
			}

			if (buffer[34U] == 0x01U) {
				::fprintf(stdout, "Received end of transmission\n");
				watchdogTimer.stop();
			}
		}

		// Refresh/add repeaters based on their polls
		std::string callsign;
		in_addr address;
		unsigned int port;
		bool ret = network.readPoll(callsign, address, port);
		if (ret) {
			CYSFRepeater* rpt = findRepeater(callsign);
			if (rpt == NULL) {
				::fprintf(stdout, "Adding %s\n", callsign.c_str());
				rpt = new CYSFRepeater;
				rpt->m_timer.start();
				rpt->m_callsign = callsign;
				rpt->m_address  = address;
				rpt->m_port     = port;
				m_repeaters.push_back(rpt);
			} else {
				rpt->m_timer.start();
				rpt->m_address = address;
				rpt->m_port    = port;
			}
		}

		unsigned int ms = stopWatch.elapsed();
		stopWatch.start();

		network.clock(ms);

		pollTimer.clock(ms);
		if (pollTimer.hasExpired()) {
			for (std::vector<CYSFRepeater*>::const_iterator it = m_repeaters.begin(); it != m_repeaters.end(); ++it)
				network.writePoll((*it)->m_address, (*it)->m_port);
			pollTimer.start();
		}

		// Remove any repeaters that haven't reported for a while
		for (std::vector<CYSFRepeater*>::iterator it = m_repeaters.begin(); it != m_repeaters.end(); ++it)
			(*it)->m_timer.clock(ms);

		for (std::vector<CYSFRepeater*>::iterator it = m_repeaters.begin(); it != m_repeaters.end(); ++it) {
			if ((*it)->m_timer.hasExpired()) {
				::fprintf(stdout, "Removing %s\n", (*it)->m_callsign.c_str());
				m_repeaters.erase(it);
				break;
			}
		}

		watchdogTimer.clock(ms);
		if (watchdogTimer.isRunning() && watchdogTimer.hasExpired()) {
			::fprintf(stdout, "Watchdog has expired\n");
			watchdogTimer.stop();
		}

		if (ms < 5U) {
#if defined(_WIN32) || defined(_WIN64)
			::Sleep(5UL);		// 5ms
#else
			::usleep(5000);		// 5ms
#endif
		}
	}

	network.close();
}

CYSFRepeater* CYSFReflector::findRepeater(const std::string& callsign) const
{
	for (std::vector<CYSFRepeater*>::const_iterator it = m_repeaters.begin(); it != m_repeaters.end(); ++it) {
		if ((*it)->m_callsign == callsign)
			return *it;
	}

	return NULL;
}
