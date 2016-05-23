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

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <sys/time.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <ctime>
#include <cstring>

int main(int argc, char** argv)
{
	if (argc == 1) {
		::fprintf(stderr, "Usage: YSFReflector <port> [log file]\n");
		return 1;
	}

	unsigned int port = ::atoi(argv[1]);
	if (port == 0U) {
		::fprintf(stderr, "YSFReflector: invalid port number\n");
		return 1;
	}

	FILE* fp = NULL;
	if (argc > 2) {
		fp = ::fopen(argv[2], "wt");
		if (fp == NULL) {
			::fprintf(stderr, "YSFReflector: cannot open the logging file - %s\n", argv[2]);
			return 1;
		}
	}

	CYSFReflector Reflector(port, fp);
	Reflector.run();

	if (fp != NULL)
		::fclose(fp);

	return 0;
}

CYSFReflector::CYSFReflector(unsigned int port, FILE* fp) :
m_port(port),
m_repeaters(),
m_fp(fp)
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

	log("Starting YSFReflector-%s", VERSION);

	CTimer watchdogTimer(1000U, 0U, 1500U);

	unsigned char tag[YSF_CALLSIGN_LENGTH];
	unsigned char src[YSF_CALLSIGN_LENGTH];
	unsigned char dst[YSF_CALLSIGN_LENGTH];

	for (;;) {
		unsigned char buffer[200U];

		unsigned int len = network.readData(buffer);
		if (len > 0U) {
			if (!watchdogTimer.isRunning()) {
				::memcpy(tag, buffer + 4U, YSF_CALLSIGN_LENGTH);

				if (::memcmp(buffer + 14U, "          ", YSF_CALLSIGN_LENGTH) != 0)
					::memcpy(src, buffer + 14U, YSF_CALLSIGN_LENGTH);
				else
					::memcpy(src, "??????????", YSF_CALLSIGN_LENGTH);

				if (::memcmp(buffer + 24U, "          ", YSF_CALLSIGN_LENGTH) != 0)
					::memcpy(dst, buffer + 24U, YSF_CALLSIGN_LENGTH);
				else
					::memcpy(dst, "??????????", YSF_CALLSIGN_LENGTH);

				log("Received data from %10.10s to %10.10s at %10.10s", src, dst, buffer + 4U);
			} else {
				if (::memcmp(tag, buffer + 4U, YSF_CALLSIGN_LENGTH) == 0) {
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
						log("Received data from %10.10s to %10.10s at %10.10s", src, dst, buffer + 4U);
				}
			}

			// Only accept transmission from an already accepted repeater
			if (::memcmp(tag, buffer + 4U, YSF_CALLSIGN_LENGTH) == 0) {
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
					log("Received end of transmission");
					watchdogTimer.stop();
				}
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
				log("Adding %s", callsign.c_str());
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
				log("Removing %s", (*it)->m_callsign.c_str());
				m_repeaters.erase(it);
				break;
			}
		}

		watchdogTimer.clock(ms);
		if (watchdogTimer.isRunning() && watchdogTimer.hasExpired()) {
			log("Network watchdog has expired");
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

void CYSFReflector::log(const char* text, ...)
{
	char buffer[300U];
#if defined(_WIN32) || defined(_WIN64)
	SYSTEMTIME st;
	::GetSystemTime(&st);

	::sprintf(buffer, "%04u-%02u-%02u %02u:%02u:%02u ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
#else
	struct timeval now;
	::gettimeofday(&now, NULL);

	struct tm* tm = ::gmtime(&now.tv_sec);

	::sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
#endif

	va_list vl;
	va_start(vl, text);

	::vsprintf(buffer + ::strlen(buffer), text, vl);

	va_end(vl);

	if (m_fp != NULL) {
		::fprintf(m_fp, "%s\n", buffer);
		::fflush(m_fp);
	}

	::fprintf(stdout, "%s\n", buffer);
	::fflush(stdout);
}