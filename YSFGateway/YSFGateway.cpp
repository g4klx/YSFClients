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

#include "YSFGateway.h"
#include "StopWatch.h"
#include "Version.h"
#include "YSFFICH.h"
#include "Thread.h"
#include "Timer.h"
#include "Log.h"

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
const char* DEFAULT_INI_FILE = "YSFGateway.ini";
#else
const char* DEFAULT_INI_FILE = "/etc/YSFGateway.ini";
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv)
{
	const char* iniFile = DEFAULT_INI_FILE;
	if (argc > 1) {
		for (int currentArg = 1; currentArg < argc; ++currentArg) {
			std::string arg = argv[currentArg];
			if ((arg == "-v") || (arg == "--version")) {
				::fprintf(stdout, "YSFGateway version %s\n", VERSION);
				return 0;
			} else if (arg.substr(0, 1) == "-") {
				::fprintf(stderr, "Usage: YSFGateway [-v|--version] [filename]\n");
				return 1;
			} else {
				iniFile = argv[currentArg];
			}
		}
	}

	CYSFGateway* gateway = new CYSFGateway(std::string(iniFile));

	int ret = gateway->run();

	delete gateway;

	return ret;
}

CYSFGateway::CYSFGateway(const std::string& configFile) :
m_conf(configFile),
m_gps(NULL),
m_hosts(NULL),
m_wiresX(NULL),
m_netNetwork(NULL),
m_linked(false)
{
}

CYSFGateway::~CYSFGateway()
{
}

int CYSFGateway::run()
{
	bool ret = m_conf.read();
	if (!ret) {
		::fprintf(stderr, "YSFGateway: cannot read the .ini file\n");
		return 1;
	}

	ret = ::LogInitialise(m_conf.getLogFilePath(), m_conf.getLogFileRoot(), m_conf.getLogFileLevel(), m_conf.getLogDisplayLevel());
	if (!ret) {
		::fprintf(stderr, "YSFGateway: unable to open the log file\n");
		return 1;
	}

#if !defined(_WIN32) && !defined(_WIN64)
	bool m_daemon = m_conf.getDaemon();
	if (m_daemon) {
		// Create new process
		pid_t pid = ::fork();
		if (pid == -1) {
			::LogWarning("Couldn't fork() , exiting");
			return -1;
		} else if (pid != 0)
			exit(EXIT_SUCCESS);

		// Create new session and process group
		if (::setsid() == -1) {
			::LogWarning("Couldn't setsid(), exiting");
			return -1;
		}

		// Set the working directory to the root directory
		if (::chdir("/") == -1) {
			::LogWarning("Couldn't cd /, exiting");
			return -1;
		}

		::close(STDIN_FILENO);
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);

		//If we are currently root...
		if (getuid() == 0) {
			struct passwd* user = ::getpwnam("mmdvm");
			if (user == NULL) {
				::LogError("Could not get the mmdvm user, exiting");
				return -1;
			}

			uid_t mmdvm_uid = user->pw_uid;
			gid_t mmdvm_gid = user->pw_gid;

			//Set user and group ID's to mmdvm:mmdvm
			if (setgid(mmdvm_gid) != 0) {
				::LogWarning("Could not set mmdvm GID, exiting");
				return -1;
			}

			if (setuid(mmdvm_uid) != 0) {
				::LogWarning("Could not set mmdvm UID, exiting");
				return -1;
			}

			//Double check it worked (AKA Paranoia) 
			if (setuid(0) != -1) {
				::LogWarning("It's possible to regain root - something is wrong!, exiting");
				return -1;
			}
		}
	}
#endif

	std::string fileName = m_conf.getNetworkHosts();
	m_hosts = new CHosts(fileName);
	m_hosts->read();

	bool debug = m_conf.getNetworkDebug();
	unsigned int rptPort = m_conf.getPort();
	unsigned int netPort = m_conf.getNetworkPort();

	CNetwork rptNetwork(rptPort, debug);
	m_netNetwork = new CNetwork(netPort, debug);

	ret = rptNetwork.open();
	if (!ret) {
		::LogError("Cannot open the repeater network port");
		return 1;
	}

	ret = m_netNetwork->open();
	if (!ret) {
		::LogError("Cannot open the reflector network port");
		return 1;
	}

	bool networkEnabled = m_conf.getNetworkEnabled();

	if (networkEnabled)
		m_wiresX = new CWiresX(&rptNetwork);

	CStopWatch stopWatch;
	stopWatch.start();

	LogMessage("Starting YSFGateway-%s", VERSION);

	createGPS();

	CTimer watchdogTimer(1000U, 0U, 500U);

	for (;;) {
		unsigned char buffer[200U];

		unsigned int len = rptNetwork.read(buffer);
		if (len > 0U) {
			watchdogTimer.start();

			CYSFFICH fich;
			bool valid = fich.decode(buffer + 35U);
			if (valid) {
				unsigned char fi = fich.getFI();
				unsigned char dt = fich.getDT();
				unsigned char fn = fich.getFN();

				if (m_wiresX != NULL) {
					WX_STATUS status = m_wiresX->process(buffer + 35U, fi, dt, fn);
					switch (status) {
					case WXS_CONNECT:
						connect(buffer + 14U);
						break;
					case WXS_DISCONNECT:
						LogMessage("Disconnect has been requested by %10.10s", buffer + 14U);
						m_netNetwork->setDestination();
						m_linked = false;
						break;
					default:
						break;
					}
				}

				if (m_gps != NULL)
					m_gps->data(buffer + 14U, buffer + 35U, fi, dt, fn);
			}

			if (buffer[34U] == 0x01U) {
				if (m_gps != NULL)
					m_gps->reset();
				watchdogTimer.stop();
			}

			if (networkEnabled && m_linked)
				m_netNetwork->write(buffer);
		}

		len = m_netNetwork->read(buffer);
		if (len > 0U) {
			if (networkEnabled && m_linked)
				rptNetwork.write(buffer);
		}

		unsigned int ms = stopWatch.elapsed();
		stopWatch.start();

		rptNetwork.clock(ms);
		m_netNetwork->clock(ms);
		if (m_gps != NULL)
			m_gps->clock(ms);
		if (m_wiresX != NULL)
			m_wiresX->clock(ms);

		watchdogTimer.clock(ms);
		if (watchdogTimer.isRunning() && watchdogTimer.hasExpired()) {
			LogMessage("Network watchdog has expired");
			if (m_gps != NULL)
				m_gps->reset();
			watchdogTimer.stop();
		}

		if (ms < 5U)
			CThread::sleep(5U);
	}

	rptNetwork.close();
	m_netNetwork->close();

	delete m_gps;
	delete m_hosts;
	delete m_netNetwork;
	delete m_wiresX;

	::LogFinalise();

	return 0;
}

void CYSFGateway::createGPS()
{
	if (!m_conf.getAPRSEnabled())
		return;

	std::string hostname = m_conf.getAPRSHostname();
	unsigned int port    = m_conf.getAPRSPort();
	std::string password = m_conf.getAPRSPassword();

	m_gps = new CGPS(hostname, port, password);
}

bool CYSFGateway::connect(const unsigned char* source)
{
	std::string reflector = m_wiresX->getReflector();

	CYSFHost* host = m_hosts->find(reflector);
	if (host == NULL) {
		LogMessage("Request made for invalid reflector %s by %10.10s", reflector.c_str(), source);
		return false;
	}
	
	std::string address = host->m_address;
	unsigned int port = host->m_port;

	in_addr addr = CUDPSocket::lookup(address);
	if (addr.s_addr == INADDR_NONE)
		return false;

	LogMessage("Connect to %s has been requested by %10.10s", reflector.c_str(), source);

	m_netNetwork->setDestination(addr, port);
	m_linked = true;
}
