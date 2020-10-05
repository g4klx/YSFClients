/*
*   Copyright (C) 2016,2018,2020 by Jonathan Naylor G4KLX
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
#include "Thread.h"
#include "Log.h"

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
const char* DEFAULT_INI_FILE = "YSFReflector.ini";
#else
const char* DEFAULT_INI_FILE = "/etc/YSFReflector.ini";
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <ctime>
#include <cstring>

int main(int argc, char** argv)
{
	const char* iniFile = DEFAULT_INI_FILE;
	if (argc > 1) {
		for (int currentArg = 1; currentArg < argc; ++currentArg) {
			std::string arg = argv[currentArg];
			if ((arg == "-v") || (arg == "--version")) {
				::fprintf(stdout, "YSFReflector version %s\n", VERSION);
				return 0;
			} else if (arg.substr(0, 1) == "-") {
				::fprintf(stderr, "Usage: YSFReflector [-v|--version] [filename]\n");
				return 1;
			} else {
				iniFile = argv[currentArg];
			}
		}
	}

	CYSFReflector* reflector = new CYSFReflector(std::string(iniFile));
	reflector->run();
	delete reflector;

	return 0;
}

CYSFReflector::CYSFReflector(const std::string& file) :
m_conf(file),
m_repeaters()
{
}

CYSFReflector::~CYSFReflector()
{
}

void CYSFReflector::run()
{
	bool ret = m_conf.read();
	if (!ret) {
		::fprintf(stderr, "YSFReflector: cannot read the .ini file\n");
		return;
	}

#if !defined(_WIN32) && !defined(_WIN64)
	bool m_daemon = m_conf.getDaemon();
	if (m_daemon) {
		// Create new process
		pid_t pid = ::fork();
		if (pid == -1) {
			::fprintf(stderr, "Couldn't fork() , exiting\n");
			return;
		} else if (pid != 0) {
			exit(EXIT_SUCCESS);
		}

		// Create new session and process group
		if (::setsid() == -1) {
			::fprintf(stderr, "Couldn't setsid(), exiting\n");
			return;
		}

		// Set the working directory to the root directory
		if (::chdir("/") == -1) {
			::fprintf(stderr, "Couldn't cd /, exiting\n");
			return;
		}

		// If we are currently root...
		if (getuid() == 0) {
			struct passwd* user = ::getpwnam("mmdvm");
			if (user == NULL) {
				::fprintf(stderr, "Could not get the mmdvm user, exiting\n");
				return;
			}

			uid_t mmdvm_uid = user->pw_uid;
			gid_t mmdvm_gid = user->pw_gid;

			// Set user and group ID's to mmdvm:mmdvm
			if (setgid(mmdvm_gid) != 0) {
				::fprintf(stderr, "Could not set mmdvm GID, exiting\n");
				return;
			}

			if (setuid(mmdvm_uid) != 0) {
				::fprintf(stderr, "Could not set mmdvm UID, exiting\n");
				return;
			}

			// Double check it worked (AKA Paranoia) 
			if (setuid(0) != -1) {
				::fprintf(stderr, "It's possible to regain root - something is wrong!, exiting\n");
				return;
			}
		}
	}
#endif

#if !defined(_WIN32) && !defined(_WIN64)
        ret = ::LogInitialise(m_daemon, m_conf.getLogFilePath(), m_conf.getLogFileRoot(), m_conf.getLogFileLevel(), m_conf.getLogDisplayLevel());
#else
        ret = ::LogInitialise(false, m_conf.getLogFilePath(), m_conf.getLogFileRoot(), m_conf.getLogFileLevel(), m_conf.getLogDisplayLevel());
#endif
	if (!ret) {
		::fprintf(stderr, "YSFReflector: unable to open the log file\n");
		return;
	}

#if !defined(_WIN32) && !defined(_WIN64)
	if (m_daemon) {
		::close(STDIN_FILENO);
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);
	}
#endif

	CNetwork network(m_conf.getNetworkPort(), m_conf.getId(), m_conf.getName(), m_conf.getDescription(), m_conf.getNetworkDebug());

	ret = network.open(m_conf.getNetworkBindAddr());
	if (!ret) {
		::LogFinalise();
		return;
	}
	
	network.setCount(0);
	
	CStopWatch stopWatch;
	stopWatch.start();

	CTimer dumpTimer(1000U, 120U);
	dumpTimer.start();

	CTimer pollTimer(1000U, 5U);
	pollTimer.start();

	LogMessage("Starting YSFReflector-%s", VERSION);

	CTimer watchdogTimer(1000U, 0U, 1500U);

	unsigned char tag[YSF_CALLSIGN_LENGTH];
	unsigned char src[YSF_CALLSIGN_LENGTH];
	unsigned char dst[YSF_CALLSIGN_LENGTH];

	for (;;) {
		unsigned char buffer[200U];
		in_addr address;
		unsigned int port;

		unsigned int len = network.readData(buffer, 200U, address, port);
		if (len > 0U) {
			CYSFRepeater* rpt = findRepeater(address, port);
			if (::memcmp(buffer, "YSFP", 4U) == 0) {
				if (rpt == NULL) {
					rpt = new CYSFRepeater;
					rpt->m_callsign = std::string((char*)(buffer + 4U), 10U);
					rpt->m_address  = address;
					rpt->m_port     = port;
					m_repeaters.push_back(rpt);
					network.setCount(m_repeaters.size());
					LogMessage("Adding %s (%s:%u)", rpt->m_callsign.c_str(), ::inet_ntoa(address), port);
				}
				rpt->m_timer.start();
				network.writePoll(address, port);
			} else if (::memcmp(buffer + 0U, "YSFU", 4U) == 0 && rpt != NULL) {
				LogMessage("Removing %s (%s:%u) unlinked", rpt->m_callsign.c_str(), ::inet_ntoa(address), port);
				for (std::vector<CYSFRepeater*>::iterator it = m_repeaters.begin(); it != m_repeaters.end(); ++it) {
					CYSFRepeater* itRpt = *it;
					if (itRpt->m_address.s_addr == rpt->m_address.s_addr && itRpt->m_port == rpt->m_port) {
						m_repeaters.erase(it);
						delete itRpt;
						break;
					}
				}
				network.setCount(m_repeaters.size());
			} else if (::memcmp(buffer + 0U, "YSFD", 4U) == 0 && rpt != NULL) {
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

					LogMessage("Received data from %10.10s to %10.10s at %10.10s", src, dst, buffer + 4U);
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
							LogMessage("Received data from %10.10s to %10.10s at %10.10s", src, dst, buffer + 4U);
					}
				}

				watchdogTimer.start();

				for (std::vector<CYSFRepeater*>::const_iterator it = m_repeaters.begin(); it != m_repeaters.end(); ++it) {
					if ((*it)->m_address.s_addr != address.s_addr || (*it)->m_port != port)
						network.writeData(buffer, (*it)->m_address, (*it)->m_port);
				}

				if ((buffer[34U] & 0x01U) == 0x01U) {
					LogMessage("Received end of transmission");
					watchdogTimer.stop();
				}
			}
		}

		unsigned int ms = stopWatch.elapsed();
		stopWatch.start();

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
			CYSFRepeater* itRpt = *it;
			if (itRpt->m_timer.hasExpired()) {
				LogMessage("Removing %s (%s:%u) disappeared", itRpt->m_callsign.c_str(), ::inet_ntoa(itRpt->m_address), itRpt->m_port);
				m_repeaters.erase(it);
				delete itRpt;
				network.setCount(m_repeaters.size());
				break;
			}
		}

		watchdogTimer.clock(ms);
		if (watchdogTimer.isRunning() && watchdogTimer.hasExpired()) {
			LogMessage("Network watchdog has expired");
			watchdogTimer.stop();
		}

		dumpTimer.clock(ms);
		if (dumpTimer.hasExpired()) {
			dumpRepeaters();
			dumpTimer.start();
		}

		if (ms < 5U)
			CThread::sleep(5U);
	}

	network.close();

	::LogFinalise();
}

CYSFRepeater* CYSFReflector::findRepeater(const in_addr& address, unsigned int port) const
{
	for (std::vector<CYSFRepeater*>::const_iterator it = m_repeaters.begin(); it != m_repeaters.end(); ++it) {
		if (address.s_addr == (*it)->m_address.s_addr && (*it)->m_port == port)
			return *it;
	}

	return NULL;
}

void CYSFReflector::dumpRepeaters() const
{
	if (m_repeaters.size() == 0U) {
		LogMessage("No repeaters/gateways linked");
		return;
	}

	LogMessage("Currently linked repeaters/gateways:");

	for (std::vector<CYSFRepeater*>::const_iterator it = m_repeaters.begin(); it != m_repeaters.end(); ++it) {
		std::string callsign = (*it)->m_callsign;
		in_addr address      = (*it)->m_address;
		unsigned int port    = (*it)->m_port;
		unsigned int timer   = (*it)->m_timer.getTimer();
		unsigned int timeout = (*it)->m_timer.getTimeout();
		LogMessage("    %s: %s:%u %u/%u", callsign.c_str(), ::inet_ntoa(address), port, timer, timeout);
	}
}
