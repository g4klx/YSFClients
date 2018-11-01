/*
*   Copyright (C) 2016,2017,2018 by Jonathan Naylor G4KLX
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
#include "UDPSocket.h"
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
#include <clocale>
#include <cmath>

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
m_callsign(),
m_suffix(),
m_conf(configFile),
m_writer(NULL),
m_gps(NULL),
m_reflectors(NULL),
m_wiresX(NULL),
m_dtmf(),
m_ysfNetwork(NULL),
m_linkType(LINK_NONE),
m_current(),
m_startup(),
m_exclude(false),
m_inactivityTimer(1000U),
m_lostTimer(1000U, 120U)
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

	setlocale(LC_ALL, "C");

#if !defined(_WIN32) && !defined(_WIN64)
	bool m_daemon = m_conf.getDaemon();
	if (m_daemon) {
		// Create new process
		pid_t pid = ::fork();
		if (pid == -1) {
			::fprintf(stderr, "Couldn't fork() , exiting\n");
			return -1;
		}
		else if (pid != 0) {
			exit(EXIT_SUCCESS);
		}

		// Create new session and process group
		if (::setsid() == -1) {
			::fprintf(stderr, "Couldn't setsid(), exiting\n");
			return -1;
		}

		// Set the working directory to the root directory
		if (::chdir("/") == -1) {
			::fprintf(stderr, "Couldn't cd /, exiting\n");
			return -1;
		}

		// If we are currently root...
		if (getuid() == 0) {
			struct passwd* user = ::getpwnam("mmdvm");
			if (user == NULL) {
				::fprintf(stderr, "Could not get the mmdvm user, exiting\n");
				return -1;
			}

			uid_t mmdvm_uid = user->pw_uid;
			gid_t mmdvm_gid = user->pw_gid;

			// Set user and group ID's to mmdvm:mmdvm
			if (setgid(mmdvm_gid) != 0) {
				::fprintf(stderr, "Could not set mmdvm GID, exiting\n");
				return -1;
			}

			if (setuid(mmdvm_uid) != 0) {
				::fprintf(stderr, "Could not set mmdvm UID, exiting\n");
				return -1;
			}

			// Double check it worked (AKA Paranoia) 
			if (setuid(0) != -1) {
				::fprintf(stderr, "It's possible to regain root - something is wrong!, exiting\n");
				return -1;
			}
		}
	}
#endif

	ret = ::LogInitialise(m_conf.getLogFilePath(), m_conf.getLogFileRoot(), m_conf.getLogFileLevel(), m_conf.getLogDisplayLevel());
	if (!ret) {
		::fprintf(stderr, "YSFGateway: unable to open the log file\n");
		return 1;
	}

#if !defined(_WIN32) && !defined(_WIN64)
	if (m_daemon) {
		::close(STDIN_FILENO);
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);
	}
#endif

	m_callsign = m_conf.getCallsign();
	m_suffix   = m_conf.getSuffix();

	bool debug            = m_conf.getNetworkDebug();
	in_addr rptAddress    = CUDPSocket::lookup(m_conf.getRptAddress());
	unsigned int rptPort  = m_conf.getRptPort();
	std::string myAddress = m_conf.getMyAddress();
	unsigned int myPort   = m_conf.getMyPort();

	CYSFNetwork rptNetwork(myAddress, myPort, m_callsign, debug);
	rptNetwork.setDestination("MMDVM", rptAddress, rptPort);

	ret = rptNetwork.open();
	if (!ret) {
		::LogError("Cannot open the repeater network port");
		::LogFinalise();
		return 1;
	}

	bool ysfNetworkEnabled = m_conf.getYSFNetworkEnabled();
	if (ysfNetworkEnabled) {
		unsigned int ysfPort = m_conf.getYSFNetworkPort();

		m_ysfNetwork = new CYSFNetwork(ysfPort, m_callsign, debug);
		ret = m_ysfNetwork->open();
		if (!ret) {
			::LogError("Cannot open the YSF reflector network port");
			::LogFinalise();
			return 1;
		}
	}

	m_inactivityTimer.setTimeout(m_conf.getNetworkInactivityTimeout() * 60U);

	std::string fileName = m_conf.getYSFNetworkHosts();
	unsigned int reloadTime = m_conf.getYSFNetworkReloadTime();

	m_reflectors = new CYSFReflectors(fileName, reloadTime);
	m_reflectors->reload();

	createWiresX(&rptNetwork);

	createGPS();

	m_startup   = m_conf.getNetworkStartup();
	bool revert = m_conf.getNetworkRevert();

	startupLinking();

	CStopWatch stopWatch;
	stopWatch.start();

	LogMessage("Starting YSFGateway-%s", VERSION);

	for (;;) {
		unsigned char buffer[200U];
		memset(buffer, 0U, 200U);

		while (rptNetwork.read(buffer) > 0U) {
			CYSFFICH fich;
			bool valid = fich.decode(buffer + 35U);
			m_exclude = false;
			if (valid) {
				unsigned char fi = fich.getFI();
				unsigned char dt = fich.getDT();
				unsigned char fn = fich.getFN();
				unsigned char ft = fich.getFT();

				// Don't send out control data
				m_exclude = (dt == YSF_DT_DATA_FR_MODE);

				processWiresX(buffer, fi, dt, fn, ft);

				processDTMF(buffer, dt);

				if (m_gps != NULL)
					m_gps->data(buffer + 14U, buffer + 35U, fi, dt, fn, ft);
			}

			if (m_ysfNetwork != NULL && m_linkType == LINK_YSF && !m_exclude) {
				m_ysfNetwork->write(buffer);
				if (::memcmp(buffer + 0U, "YSFD", 4U) == 0)
					m_inactivityTimer.start();
			}

			if ((buffer[34U] & 0x01U) == 0x01U) {
				if (m_gps != NULL)
					m_gps->reset();
				m_dtmf.reset();
				m_exclude = false;
			}
		}

		if (m_ysfNetwork != NULL) {
			while (m_ysfNetwork->read(buffer) > 0U) {
				if (m_linkType == LINK_YSF) {
					// Only pass through YSF data packets
					if (::memcmp(buffer + 0U, "YSFD", 4U) == 0)
						rptNetwork.write(buffer);

					m_lostTimer.start();
				}
			}
		}

		unsigned int ms = stopWatch.elapsed();
		stopWatch.start();

		rptNetwork.clock(ms);
		if (m_ysfNetwork != NULL)
			m_ysfNetwork->clock(ms);
		if (m_writer != NULL)
			m_writer->clock(ms);
		m_wiresX->clock(ms);

		m_inactivityTimer.clock(ms);
		if (m_inactivityTimer.isRunning() && m_inactivityTimer.hasExpired()) {
			if (revert) {
				if (m_current != m_startup) {
					if (m_linkType == LINK_YSF) {
						m_wiresX->processDisconnect();
						m_ysfNetwork->writeUnlink(3U);
						m_ysfNetwork->clearDestination();
					}

					m_current.clear();
					m_lostTimer.stop();
					m_linkType = LINK_NONE;

					startupLinking();
				}
			} else {
				if (m_linkType == LINK_YSF) {
					LogMessage("Disconnecting due to inactivity");
					m_wiresX->processDisconnect();
					m_ysfNetwork->writeUnlink(3U);
					m_ysfNetwork->clearDestination();
				}

				m_current.clear();
				m_lostTimer.stop();
				m_linkType = LINK_NONE;
			}

			m_inactivityTimer.start();
		}

		m_lostTimer.clock(ms);
		if (m_lostTimer.isRunning() && m_lostTimer.hasExpired()) {
			if (m_linkType == LINK_YSF) {
				LogWarning("Link has failed, polls lost");
				m_wiresX->processDisconnect();
				m_ysfNetwork->clearDestination();
			}

			m_current.clear();
			m_inactivityTimer.start();
			m_lostTimer.stop();
			m_linkType = LINK_NONE;
		}

		if (ms < 5U)
			CThread::sleep(5U);
	}

	rptNetwork.close();

	if (m_gps != NULL) {
		m_writer->close();
		delete m_writer;
		delete m_gps;
	}

	if (m_ysfNetwork != NULL) {
		m_ysfNetwork->close();
		delete m_ysfNetwork;
	}

	delete m_wiresX;

	::LogFinalise();

	return 0;
}

void CYSFGateway::createGPS()
{
	if (!m_conf.getAPRSEnabled())
		return;

	std::string hostname = m_conf.getAPRSServer();
	unsigned int port    = m_conf.getAPRSPort();
	std::string password = m_conf.getAPRSPassword();
	std::string suffix   = m_conf.getAPRSSuffix();
	std::string desc     = m_conf.getAPRSDescription();

	m_writer = new CAPRSWriter(m_callsign, m_suffix, password, hostname, port, suffix);

	unsigned int txFrequency = m_conf.getTxFrequency();
	unsigned int rxFrequency = m_conf.getRxFrequency();
	float latitude           = m_conf.getLatitude();
	float longitude          = m_conf.getLongitude();
	int height               = m_conf.getHeight();

	m_writer->setInfo(txFrequency, rxFrequency, latitude, longitude, height, desc);

	bool enabled = m_conf.getMobileGPSEnabled();
	if (enabled) {
	        std::string address = m_conf.getMobileGPSAddress();
	        unsigned int port   = m_conf.getMobileGPSPort();

	        m_writer->setMobileGPS(address, port);
	}

	bool ret = m_writer->open();
	if (!ret) {
	        delete m_writer;
		m_writer = NULL;
		return;
	}

	m_gps = new CGPS(m_writer);
}

void CYSFGateway::createWiresX(CYSFNetwork* rptNetwork)
{
	assert(rptNetwork != NULL);

	m_wiresX = new CWiresX(m_callsign, m_suffix, rptNetwork, *m_reflectors);

	std::string name = m_conf.getName();

	unsigned int txFrequency = m_conf.getTxFrequency();
	unsigned int rxFrequency = m_conf.getRxFrequency();
	m_wiresX->setInfo(name, txFrequency, rxFrequency);

	std::string address = m_conf.getYSFNetworkParrotAddress();
	unsigned int port = m_conf.getYSFNetworkParrotPort();
	if (port > 0U)
		m_wiresX->setParrot(address, port);

	address = m_conf.getYSFNetworkYSF2DMRAddress();
	port = m_conf.getYSFNetworkYSF2DMRPort();
	if (port > 0U)
		m_wiresX->setYSF2DMR(address, port);

	address = m_conf.getYSFNetworkYSF2NXDNAddress();
	port = m_conf.getYSFNetworkYSF2NXDNPort();
	if (port > 0U)
		m_wiresX->setYSF2NXDN(address, port);

	address = m_conf.getYSFNetworkYSF2P25Address();
	port = m_conf.getYSFNetworkYSF2P25Port();
	if (port > 0U)
		m_wiresX->setYSF2P25(address, port);

	m_reflectors->load();
	m_wiresX->start();
}

void CYSFGateway::processWiresX(const unsigned char* buffer, unsigned char fi, unsigned char dt, unsigned char fn, unsigned char ft)
{
	assert(buffer != NULL);

	WX_STATUS status = m_wiresX->process(buffer + 35U, buffer + 14U, fi, dt, fn, ft);
	switch (status) {
	case WXS_CONNECT: {
			if (m_linkType == LINK_YSF)
				m_ysfNetwork->writeUnlink(3U);

			CYSFReflector* reflector = m_wiresX->getReflector();
			LogMessage("Connect to %5.5s - \"%s\" has been requested by %10.10s", reflector->m_id.c_str(), reflector->m_name.c_str(), buffer + 14U);

			m_ysfNetwork->setDestination(reflector->m_name, reflector->m_address, reflector->m_port);
			m_ysfNetwork->writePoll(3U);

			m_current = reflector->m_id;
			m_inactivityTimer.start();
			m_lostTimer.start();
			m_linkType = LINK_YSF;
		}
		break;
	case WXS_DISCONNECT:
		if (m_linkType == LINK_YSF) {
			LogMessage("Disconnect has been requested by %10.10s", buffer + 14U);

			m_ysfNetwork->writeUnlink(3U);
			m_ysfNetwork->clearDestination();

			m_current.clear();
			m_inactivityTimer.start();
			m_lostTimer.stop();
			m_linkType = LINK_NONE;
		}
		break;
	default:
		break;
	}
}

void CYSFGateway::processDTMF(unsigned char* buffer, unsigned char dt)
{
	assert(buffer != NULL);

	WX_STATUS status = WXS_NONE;
	switch (dt) {
	case YSF_DT_VD_MODE2:
		status = m_dtmf.decodeVDMode2(buffer + 35U, (buffer[34U] & 0x01U) == 0x01U);
		break;
	default:
		break;
	}

	switch (status) {
	case WXS_CONNECT: {
			std::string id = m_dtmf.getReflector();
			CYSFReflector* reflector = m_reflectors->findById(id);
			if (reflector != NULL) {
				m_wiresX->processConnect(reflector);

				if (m_linkType == LINK_YSF)
					m_ysfNetwork->writeUnlink(3U);


				LogMessage("Connect via DTMF to %5.5s - \"%s\" has been requested by %10.10s", reflector->m_id.c_str(), reflector->m_name.c_str(), buffer + 14U);

				m_ysfNetwork->setDestination(reflector->m_name, reflector->m_address, reflector->m_port);
				m_ysfNetwork->writePoll(3U);

				m_current = id;
				m_inactivityTimer.start();
				m_lostTimer.start();
				m_linkType = LINK_YSF;
			}
		}
		break;

	case WXS_DISCONNECT:
		if (m_linkType == LINK_YSF) {
			m_wiresX->processDisconnect();

			LogMessage("Disconnect via DTMF has been requested by %10.10s", buffer + 14U);

			m_ysfNetwork->writeUnlink(3U);
			m_ysfNetwork->clearDestination();

			m_current.clear();
			m_inactivityTimer.stop();
			m_lostTimer.stop();
			m_linkType = LINK_NONE;
		}
		break;
	default:
		break;
	}
}

void CYSFGateway::startupLinking()
{
	if (!m_startup.empty()) {
		if (m_ysfNetwork != NULL) {
			m_current.clear();
			m_inactivityTimer.stop();
			m_lostTimer.stop();
			m_linkType = LINK_NONE;

			CYSFReflector* reflector = m_reflectors->findByName(m_startup);
			if (reflector != NULL) {
				LogMessage("Automatic (re-)connection to %5.5s - \"%s\"", reflector->m_id.c_str(), reflector->m_name.c_str());

				m_wiresX->setReflector(reflector);

				m_ysfNetwork->setDestination(reflector->m_name, reflector->m_address, reflector->m_port);
				m_ysfNetwork->writePoll(3U);

				m_current = m_startup;
				m_inactivityTimer.start();
				m_lostTimer.start();
				m_linkType = LINK_YSF;
			}
		}
	}
	if (m_startup.empty())
		 LogMessage("No connection startup");
}
