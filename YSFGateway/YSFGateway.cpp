/*
*   Copyright (C) 2016-2020,2023 by Jonathan Naylor G4KLX
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
#include "MQTTConnection.h"
#include "UDPSocket.h"
#include "StopWatch.h"
#include "Version.h"
#include "YSFFICH.h"
#include "Thread.h"
#include "Timer.h"
#include "Utils.h"
#include "Log.h"
#include "GitVersion.h"

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

// In Log.cpp
extern CMQTTConnection* m_mqtt;

static CYSFGateway* gateway = NULL;

static bool m_killed = false;
static int  m_signal = 0;

#if !defined(_WIN32) && !defined(_WIN64)
static void sigHandler(int signum)
{
	m_killed = true;
	m_signal = signum;
}
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <clocale>
#include <cmath>
#include <algorithm>

int main(int argc, char** argv)
{
	const char* iniFile = DEFAULT_INI_FILE;
	if (argc > 1) {
		for (int currentArg = 1; currentArg < argc; ++currentArg) {
			std::string arg = argv[currentArg];
			if ((arg == "-v") || (arg == "--version")) {
				::fprintf(stdout, "YSFGateway version %s git #%.7s\n", VERSION, gitversion);
				return 0;
			} else if (arg.substr(0, 1) == "-") {
				::fprintf(stderr, "Usage: YSFGateway [-v|--version] [filename]\n");
				return 1;
			} else {
				iniFile = argv[currentArg];
			}
		}
	}

#if !defined(_WIN32) && !defined(_WIN64)
	::signal(SIGINT,  sigHandler);
	::signal(SIGTERM, sigHandler);
	::signal(SIGHUP,  sigHandler);
#endif

	int ret = 0;

	do {
		m_killed = false;
		m_signal = 0;

		gateway = new CYSFGateway(std::string(iniFile));
		ret = gateway->run();

		delete gateway;

		if (m_signal == 2)
			::LogInfo("YSFGateway-%s exited on receipt of SIGINT", VERSION);

		if (m_signal == 15)
			::LogInfo("YSFGateway-%s exited on receipt of SIGTERM", VERSION);

		if (m_signal == 1)
			::LogInfo("YSFGateway-%s restarted on receipt of SIGHUP", VERSION);

	} while (m_signal == 1);

	::LogFinalise();

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
m_fcsNetwork(NULL),
m_linkType(LINK_NONE),
m_current(),
m_startup(),
m_options(),
m_exclude(false),
m_inactivityTimer(1000U),
m_lostTimer(1000U, 120U),
m_fcsNetworkEnabled(false)
{
	CUDPSocket::startup();
}

CYSFGateway::~CYSFGateway()
{
	CUDPSocket::shutdown();
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
		} else if (pid != 0) {
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

#if !defined(_WIN32) && !defined(_WIN64)
	if (m_daemon) {
		::close(STDIN_FILENO);
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);
	}
#endif
	::LogInitialise(m_conf.getLogDisplayLevel(), m_conf.getLogMQTTLevel());

	std::vector<std::pair<std::string, void (*)(const unsigned char*, unsigned int)>> subscriptions;
	if (m_conf.getRemoteCommandsEnabled())
		subscriptions.push_back(std::make_pair("command", CYSFGateway::onCommand));

	m_mqtt = new CMQTTConnection(m_conf.getMQTTAddress(), m_conf.getMQTTPort(), m_conf.getMQTTName(), subscriptions, m_conf.getMQTTKeepalive());
	ret = m_mqtt->open();
	if (!ret)
		return 1;

	m_callsign = m_conf.getCallsign();
	m_suffix   = m_conf.getSuffix();

	bool debug = m_conf.getDebug();
	sockaddr_storage rptAddr;
	unsigned int rptAddrLen;
	if (CUDPSocket::lookup(m_conf.getRptAddress(), m_conf.getRptPort(), rptAddr, rptAddrLen) != 0) {
		::LogError("Cannot find the address of the MMDVM Host");
		return 1;
	}

	std::string myAddress = m_conf.getMyAddress();
	unsigned short myPort = m_conf.getMyPort();
	CYSFNetwork rptNetwork(myAddress, myPort, m_callsign, debug);

	ret = rptNetwork.setDestination("MMDVM", rptAddr, rptAddrLen);
	if (!ret) {
		::LogError("Cannot open the repeater network port");
		return 1;
	}

	bool ysfNetworkEnabled = m_conf.getYSFNetworkEnabled();
	if (ysfNetworkEnabled) {
		unsigned short ysfPort = m_conf.getYSFNetworkPort();
		m_ysfNetwork = new CYSFNetwork(ysfPort, m_callsign, debug);
	}

	m_fcsNetworkEnabled = m_conf.getFCSNetworkEnabled();
	if (m_fcsNetworkEnabled) {
		unsigned int txFrequency = m_conf.getTxFrequency();
		unsigned int rxFrequency = m_conf.getRxFrequency();
		std::string locator = calculateLocator();
		unsigned int id = m_conf.getId();

		unsigned short fcsPort = m_conf.getFCSNetworkPort();

		m_fcsNetwork = new CFCSNetwork(fcsPort, m_callsign, rxFrequency, txFrequency, locator, id, debug);
		ret = m_fcsNetwork->open();
		if (!ret) {
			::LogError("Cannot open the FCS reflector network port");
			return 1;
		}
	}

	m_inactivityTimer.setTimeout(m_conf.getNetworkInactivityTimeout() * 60U);

	std::string fileName = m_conf.getYSFNetworkHosts();
	unsigned int reloadTime = m_conf.getYSFNetworkReloadTime();
	bool wiresXMakeUpper = m_conf.getWiresXMakeUpper();

	m_reflectors = new CYSFReflectors(fileName, reloadTime, wiresXMakeUpper);
	m_reflectors->reload();

	createWiresX(&rptNetwork);

	createGPS();

	m_startup   = m_conf.getNetworkStartup();
	m_options   = m_conf.getNetworkOptions();
	bool revert = m_conf.getNetworkRevert();
	bool wiresXCommandPassthrough = m_conf.getWiresXCommandPassthrough();

	linking("startup");

	CStopWatch stopWatch;
	stopWatch.start();

	LogInfo("YSFGateway-%s is starting", VERSION);
	LogInfo("Built %s %s (GitID #%.7s)", __TIME__, __DATE__, gitversion);

	writeJSONStatus("YSFGateway is starting");

	while (!m_killed) {
		unsigned char buffer[200U];
		memset(buffer, 0U, 200U);

		while (rptNetwork.read(buffer) > 0U) {
			CYSFFICH fich;
			bool valid = fich.decode(buffer + 35U);
			m_exclude = false;
			if (valid) {
				unsigned char dt = fich.getDT();
				bool wx_tmp = false;

				CYSFReflector* reflector = m_wiresX->getReflector();
				if (reflector != NULL)
					wx_tmp = reflector->m_wiresX;

				if (m_ysfNetwork != NULL && m_linkType == LINK_YSF && wiresXCommandPassthrough && wx_tmp) {
					processDTMF(buffer, dt);
					processWiresX(buffer, fich, true, wiresXCommandPassthrough);
				} else {
					processDTMF(buffer, dt);
					processWiresX(buffer, fich, false, wiresXCommandPassthrough);
					reflector = m_wiresX->getReflector(); //reflector may have changed
					if (reflector != NULL)
						wx_tmp = reflector->m_wiresX;
					else
						wx_tmp = false;
					if (m_ysfNetwork != NULL && m_linkType == LINK_YSF && wx_tmp)
						m_exclude = (dt == YSF_DT_DATA_FR_MODE);
				}

				if (m_gps != NULL)
					m_gps->data(buffer + 14U, buffer + 35U, fich);
			}

			if (m_ysfNetwork != NULL && m_linkType == LINK_YSF && !m_exclude) {
				if (::memcmp(buffer + 0U, "YSFD", 4U) == 0) {
					m_ysfNetwork->write(buffer);
					m_inactivityTimer.start();
				}
			}

			if (m_fcsNetwork != NULL && m_linkType == LINK_FCS && !m_exclude) {
				if (::memcmp(buffer + 0U, "YSFD", 4U) == 0) {
					m_fcsNetwork->write(buffer);
					m_inactivityTimer.start();
				}
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
					if (::memcmp(buffer + 0U, "YSFD", 4U) == 0 && !m_wiresX->isBusy())
						rptNetwork.write(buffer);

					m_lostTimer.start();
				}
			}
		}

		if (m_fcsNetwork != NULL) {
			while (m_fcsNetwork->read(buffer) > 0U) {
				if (m_linkType == LINK_FCS) {
					// Only pass through YSF data packets
					if (::memcmp(buffer + 0U, "YSFD", 4U) == 0 && !m_wiresX->isBusy())
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
		if (m_fcsNetwork != NULL)
			m_fcsNetwork->clock(ms);
		if (m_writer != NULL)
			m_writer->clock(ms);
		m_wiresX->clock(ms);

		m_inactivityTimer.clock(ms);
		if (m_inactivityTimer.isRunning() && m_inactivityTimer.hasExpired()) {
			if (revert) {
				if (m_current != m_startup) {
					if (m_linkType == LINK_YSF) {
						writeJSONUnlinked("timer");
						m_wiresX->processDisconnect();
						m_ysfNetwork->writeUnlink(3U);
						m_ysfNetwork->clearDestination();
					}

					if (m_linkType == LINK_FCS) {
						writeJSONUnlinked("timer");
						m_fcsNetwork->writeUnlink(3U);
						m_fcsNetwork->clearDestination();
					}

					m_current.clear();
					m_lostTimer.stop();
					m_linkType = LINK_NONE;

					linking("timer");
				}
			} else {
				if (m_linkType == LINK_YSF) {
					LogMessage("Disconnecting due to inactivity");
					writeJSONUnlinked("timer");
					m_wiresX->processDisconnect();
					m_ysfNetwork->writeUnlink(3U);
					m_ysfNetwork->clearDestination();
				}

				if (m_linkType == LINK_FCS) {
					LogMessage("Disconnecting due to inactivity");
					writeJSONUnlinked("timer");
					m_fcsNetwork->writeUnlink(3U);
					m_fcsNetwork->clearDestination();
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

			if (m_fcsNetwork != NULL) {
				LogWarning("Link has failed, polls lost");
				m_fcsNetwork->clearDestination();
			}

			m_current.clear();
			m_inactivityTimer.start();
			m_lostTimer.stop();
			m_linkType = LINK_NONE;
		}

		if (ms < 5U)
			CThread::sleep(5U);
	}

	LogInfo("YSFGateway is stopping");
	writeJSONStatus("YSFGateway is stopping");

	rptNetwork.clearDestination();

	if (m_gps != NULL) {
		m_writer->close();
		delete m_writer;
		delete m_gps;
	}

	if (m_ysfNetwork != NULL) {
		m_ysfNetwork->clearDestination();
		delete m_ysfNetwork;
	}

	if (m_fcsNetwork != NULL) {
		m_fcsNetwork->close();
		delete m_fcsNetwork;
	}

	delete m_wiresX;

	return 0;
}

void CYSFGateway::createGPS()
{
	if (!m_conf.getAPRSEnabled())
		return;

	std::string suffix  = m_conf.getAPRSSuffix();
	bool debug          = m_conf.getDebug();

	m_writer = new CAPRSWriter(m_callsign, m_suffix, suffix, debug);

	unsigned int txFrequency = m_conf.getTxFrequency();
	unsigned int rxFrequency = m_conf.getRxFrequency();
	std::string desc         = m_conf.getAPRSDescription();
	std::string symbol       = m_conf.getAPRSSymbol();

	m_writer->setInfo(txFrequency, rxFrequency, desc, symbol);

	bool enabled = m_conf.getGPSDEnabled();
	if (enabled) {
		std::string address = m_conf.getGPSDAddress();
		std::string port    = m_conf.getGPSDPort();

		m_writer->setGPSDLocation(address, port);
	} else {
		float latitude  = m_conf.getLatitude();
		float longitude = m_conf.getLongitude();
		int height      = m_conf.getHeight();

		m_writer->setStaticLocation(latitude, longitude, height);
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
	unsigned short port = m_conf.getYSFNetworkParrotPort();
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

	address = m_conf.getYSFNetworkYSFDirectAddress();
	port = m_conf.getYSFNetworkYSFDirectPort();
	if (port > 0U)
		m_wiresX->setYSFDirect(address, port);

	std::string filename = m_conf.getFCSNetworkFile();
	if (m_fcsNetworkEnabled)
		readFCSRoomsFile(filename);

	m_reflectors->load();
	m_wiresX->start();
}

void CYSFGateway::processWiresX(const unsigned char* buffer, const CYSFFICH& fich, bool dontProcessWiresXLocal, bool wiresXCommandPassthrough)
{
	assert(buffer != NULL);

	WX_STATUS status = m_wiresX->process(buffer + 35U, buffer + 14U, fich, dontProcessWiresXLocal);
	switch (status) {
	case WXS_CONNECT_YSF: {
			if (m_linkType == LINK_YSF) {
				writeJSONUnlinked("user");
				m_ysfNetwork->writeUnlink(3U);
			}

			if (m_linkType == LINK_FCS) {
				writeJSONUnlinked("user");
				m_fcsNetwork->writeUnlink(3U);
				m_fcsNetwork->clearDestination();
			}

			CYSFReflector* reflector = m_wiresX->getReflector();
			LogMessage("Connect to %5.5s - \"%s\" has been requested by %10.10s", reflector->m_id.c_str(), reflector->m_name.c_str(), buffer + 14U);
			writeJSONLinking("user", "ysf", reflector->m_name);

			m_ysfNetwork->setDestination(reflector->m_name, reflector->m_addr, reflector->m_addrLen);
			m_ysfNetwork->writePoll(3U);

			m_current = reflector->m_id;
			m_inactivityTimer.start();
			m_lostTimer.start();
			m_linkType = LINK_YSF;

			// If we are linking to a YSF2xxx mode, send the YSF2xxx gateway the link command too
			if (reflector->m_wiresX && wiresXCommandPassthrough) {
				LogMessage("Forward WiresX Connect to \"%s\"", reflector->m_name.c_str());
				m_wiresX->sendConnect(m_ysfNetwork);
			}
		}
		break;
	case WXS_CONNECT_FCS: {
			if (m_linkType == LINK_YSF) {
				writeJSONUnlinked("user");
				m_ysfNetwork->writeUnlink(3U);
				m_ysfNetwork->clearDestination();
			}

			if (m_linkType == LINK_FCS) {
				writeJSONUnlinked("user");
				m_fcsNetwork->writeUnlink(3U);
			}

			m_current.clear();
			m_inactivityTimer.start();
			m_lostTimer.stop();
			m_linkType = LINK_NONE;

			CYSFReflector* reflector = m_wiresX->getReflector();
			LogMessage("Connect to %s - \"%s\" has been requested by %10.10s", reflector->m_id.c_str(), reflector->m_name.c_str(), buffer + 14U);
			writeJSONLinking("user", "fcs", reflector->m_name);

			std::string name = reflector->m_name;
			name.resize(8U, '0');

			bool ok = m_fcsNetwork->writeLink(name);
			if (ok) {
				m_current = name;
				m_lostTimer.start();
				m_linkType = LINK_FCS;
			} else {
				LogMessage("Unknown reflector - %s", name.c_str());
			}
		}
		break;
	case WXS_DISCONNECT:
		if (m_linkType == LINK_YSF) {
			LogMessage("Disconnect has been requested by %10.10s", buffer + 14U);
			if ( (wiresXCommandPassthrough) && (::memcmp(buffer + 0U, "YSFD", 4U) == 0) ) {
				// Send the disconnect to the YSF2xxx gateway too
				m_ysfNetwork->write(buffer);
			}

			writeJSONUnlinked("user");
			m_ysfNetwork->writeUnlink(3U);
			m_ysfNetwork->clearDestination();

			m_current.clear();
			m_inactivityTimer.start();
			m_lostTimer.stop();
			m_linkType = LINK_NONE;
		}
		if (m_linkType == LINK_FCS) {
			LogMessage("Disconnect has been requested by %10.10s", buffer + 14U);

			writeJSONUnlinked("user");
			m_fcsNetwork->writeUnlink(3U);
			m_fcsNetwork->clearDestination();

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
	case WXS_CONNECT_YSF: {
			std::string id = m_dtmf.getReflector();
			CYSFReflector* reflector = m_reflectors->findById(id);
			if (reflector != NULL) {
				m_wiresX->processConnect(reflector);

				if (m_linkType == LINK_YSF) {
					writeJSONUnlinked("user");
					m_ysfNetwork->writeUnlink(3U);
				}

				if (m_linkType == LINK_FCS) {
					writeJSONUnlinked("user");
					m_fcsNetwork->writeUnlink(3U);
					m_fcsNetwork->clearDestination();
				}

				LogMessage("Connect via DTMF to %5.5s - \"%s\" has been requested by %10.10s", reflector->m_id.c_str(), reflector->m_name.c_str(), buffer + 14U);
				writeJSONLinking("user", "ysf", reflector->m_name);

				m_ysfNetwork->setDestination(reflector->m_name, reflector->m_addr, reflector->m_addrLen);
				m_ysfNetwork->writePoll(3U);

				m_current = id;
				m_inactivityTimer.start();
				m_lostTimer.start();
				m_linkType = LINK_YSF;
			}
		}
		break;
	case WXS_CONNECT_FCS: {
			std::string raw = m_dtmf.getReflector();
			std::string id = "FCS00";
			std::string idShort = "FCS";
			if (raw.length() == 2U) {
				id += raw.at(0U) + std::string("0") + raw.at(1U);
			} else if (raw.length() == 3U) {
				id += raw;
			} else if (raw.length() == 5U) {
				id = idShort;
				id += raw;
			} else {
				LogWarning("Nonsense from the DTMF decoder - \"%s\"", raw.c_str());
				return;
			}

			if (m_linkType == LINK_YSF) {
				writeJSONUnlinked("user");
				m_wiresX->processDisconnect();
				m_ysfNetwork->writeUnlink(3U);
				m_ysfNetwork->clearDestination();
			}

			if (m_linkType == LINK_FCS) {
				writeJSONUnlinked("user");
				m_fcsNetwork->writeUnlink(3U);
			}

			m_current.clear();
			m_inactivityTimer.stop();
			m_lostTimer.stop();
			m_linkType = LINK_NONE;

			LogMessage("Connect via DTMF to %s has been requested by %10.10s", id.c_str(), buffer + 14U);

			bool ok = m_fcsNetwork->writeLink(id);
			if (ok) {
				writeJSONLinking("user", "fcs", id);

				m_current = id;
				m_inactivityTimer.start();
				m_lostTimer.start();
				m_linkType = LINK_FCS;
			} else {
				LogMessage("Unknown reflector - %s", id.c_str());
			}
		}
		break;
	case WXS_DISCONNECT:
		if (m_linkType == LINK_YSF) {
			m_wiresX->processDisconnect();

			LogMessage("Disconnect via DTMF has been requested by %10.10s", buffer + 14U);
			writeJSONUnlinked("user");

			m_ysfNetwork->writeUnlink(3U);
			m_ysfNetwork->clearDestination();

			m_current.clear();
			m_inactivityTimer.start();
			m_lostTimer.stop();
			m_linkType = LINK_NONE;
		}

		if (m_linkType == LINK_FCS) {
			LogMessage("Disconnect via DTMF has been requested by %10.10s", buffer + 14U);
			writeJSONUnlinked("user");

			m_fcsNetwork->writeUnlink(3U);
			m_fcsNetwork->clearDestination();

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

std::string CYSFGateway::calculateLocator()
{
	std::string locator;

	float latitude  = m_conf.getLatitude();
	float longitude = m_conf.getLongitude();

	if (latitude < -90.0F || latitude > 90.0F)
		return "AA00AA";

	if (longitude < -360.0F || longitude > 360.0F)
		return "AA00AA";

	latitude += 90.0F;

	if (longitude > 180.0F)
		longitude -= 360.0F;

	if (longitude < -180.0F)
		longitude += 360.0F;

	longitude += 180.0F;

	float lon = ::floor(longitude / 20.0F);
	float lat = ::floor(latitude  / 10.0F);

	locator += 'A' + (unsigned int)lon;
	locator += 'A' + (unsigned int)lat;

	longitude -= lon * 20.0F;
	latitude  -= lat * 10.0F;

	lon = ::floor(longitude / 2.0F);
	lat = ::floor(latitude  / 1.0F);

	locator += '0' + (unsigned int)lon;
	locator += '0' + (unsigned int)lat;

	longitude -= lon * 2.0F;
	latitude  -= lat * 1.0F;

	lon = ::floor(longitude / (2.0F / 24.0F));
	lat = ::floor(latitude  / (1.0F / 24.0F));

	locator += 'A' + (unsigned int)lon;
	locator += 'A' + (unsigned int)lat;

	return locator;
}

void CYSFGateway::linking(const std::string& reason)
{
	if (!m_startup.empty()) {
		if (m_startup.substr(0U, 3U) == "FCS" && m_fcsNetwork != NULL) {
			m_current.clear();
			m_inactivityTimer.stop();
			m_lostTimer.stop();
			m_linkType = LINK_NONE;

			bool ok = m_fcsNetwork->writeLink(m_startup);
			m_fcsNetwork->setOptions(m_options);

			if (ok) {
				writeJSONLinking(reason, "fcs", m_startup);
				LogMessage("Automatic (re-)connection to %s", m_startup.c_str());

				m_current = m_startup;
				m_inactivityTimer.start();
				m_lostTimer.start();
				m_linkType = LINK_FCS;
			} else {
				LogMessage("Unknown reflector - %s", m_startup.c_str());
			}
		} else if (m_ysfNetwork != NULL) {
			m_current.clear();
			m_inactivityTimer.stop();
			m_lostTimer.stop();
			m_linkType = LINK_NONE;

			CYSFReflector* reflector = m_reflectors->findByName(m_startup);
			if (reflector == NULL)
				reflector = m_reflectors->findById(m_startup);
			if (reflector != NULL) {
				writeJSONLinking(reason, "ysf", reflector->m_name);
				LogMessage("Automatic (re-)connection to %5.5s - \"%s\"", reflector->m_id.c_str(), reflector->m_name.c_str());

				m_ysfNetwork->setOptions(m_options);

				m_wiresX->setReflector(reflector);

				m_ysfNetwork->setDestination(reflector->m_name, reflector->m_addr, reflector->m_addrLen);
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

void CYSFGateway::readFCSRoomsFile(const std::string& filename)
{
	FILE* fp = ::fopen(filename.c_str(), "rt");
	if (fp == NULL)
		return;

	unsigned int count = 0U;

	char buffer[200U];
	while (::fgets(buffer, 200, fp) != NULL) {
		if (buffer[0U] == '#')
			continue;

		char* p1 = ::strtok(buffer, ";");
		char* p2 = ::strtok(NULL, ";");

		if (p1 != NULL && p2 != NULL) {
			m_wiresX->addFCSRoom(p1, p2);
			count++;
		}
	}

	::fclose(fp);

	LogInfo("Loaded %u FCS room descriptions", count);
}

void CYSFGateway::writeCommand(const std::string& command)
{
	if (command.substr(0, 7) == "LinkYSF" && command.length() > 8) {
		std::string id = command.substr(8);

		// Left trim
		id.erase(id.begin(), std::find_if(id.begin(), id.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
		CYSFReflector* reflector = m_reflectors->findById(id);

		if (reflector == NULL)
			reflector = m_reflectors->findByName(id);

		if (reflector != NULL) {
			m_wiresX->processConnect(reflector);

			if (m_linkType == LINK_YSF) {
				writeJSONUnlinked("remote");
				m_ysfNetwork->writeUnlink(3U);
			}

			if (m_linkType == LINK_FCS) {
				writeJSONUnlinked("remote");
				m_fcsNetwork->writeUnlink(3U);
				m_fcsNetwork->clearDestination();
			}

			LogMessage("Connect by remote command to %5.5s - \"%s\"", reflector->m_id.c_str(), reflector->m_name.c_str());
			writeJSONLinking("remote", "ysf", reflector->m_name);

			m_ysfNetwork->setDestination(reflector->m_name, reflector->m_addr, reflector->m_addrLen);
			m_ysfNetwork->writePoll(3U);

			m_current = id;
			m_inactivityTimer.start();
			m_lostTimer.start();
			m_linkType = LINK_YSF;
		} else {
			LogWarning("Invalid YSF reflector id/name - \"%s\"", id.c_str());
			return;
		}
	} else if (command.substr(0, 7) == "LinkFCS" && command.length() > 8) {
		std::string raw = command.substr(8);

		// Left trim
		raw.erase(raw.begin(), std::find_if(raw.begin(), raw.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
		std::string id = "FCS00";
		std::string idShort = "FCS";

		if (raw.length() == 3U) {
			id += raw;
		} else if (raw.length() == 5U) {
			id = idShort;
			id += raw;
		} else {
			LogWarning("Invalid FCS reflector id - \"%s\"", raw.c_str());
			return;
		}

		if (m_linkType == LINK_YSF) {
			writeJSONUnlinked("remote");
			m_wiresX->processDisconnect();
			m_ysfNetwork->writeUnlink(3U);
			m_ysfNetwork->clearDestination();
		}

		if (m_linkType == LINK_FCS) {
			writeJSONUnlinked("remote");
			m_fcsNetwork->writeUnlink(3U);
		}

		m_current.clear();
		m_inactivityTimer.stop();
		m_lostTimer.stop();
		m_linkType = LINK_NONE;

		LogMessage("Connect by remote command to %s", id.c_str());

		bool ok = m_fcsNetwork->writeLink(id);
		if (ok) {
			writeJSONLinking("remote", "fcs", id);

			m_current = id;
			m_inactivityTimer.start();
			m_lostTimer.start();
			m_linkType = LINK_FCS;
		} else {
			LogMessage("Unknown reflector - %s", id.c_str());
		}
	} else if (command.substr(0, 6) == "UnLink") {
		if (m_linkType == LINK_YSF) {
			m_wiresX->processDisconnect();

			LogMessage("Disconnect by remote command");
			writeJSONUnlinked("remote");

			m_ysfNetwork->writeUnlink(3U);
			m_ysfNetwork->clearDestination();

			m_current.clear();
			m_inactivityTimer.stop();
			m_lostTimer.stop();
			m_linkType = LINK_NONE;
		}
		if (m_linkType == LINK_FCS) {
			LogMessage("Disconnect by remote command");
			writeJSONUnlinked("remote");

			m_fcsNetwork->writeUnlink(3U);
			m_fcsNetwork->clearDestination();

			m_current.clear();
			m_inactivityTimer.stop();
			m_lostTimer.stop();
			m_linkType = LINK_NONE;
		}
	} else if (command.substr(0, 6) == "status") {
		std::string state = std::string("ysf:") + (((m_ysfNetwork == NULL) && (m_fcsNetwork == NULL)) ? "n/a" : ((m_linkType != LINK_NONE) ? "conn" : "disc"));
		m_mqtt->publish("response", state);
	} else if (command.substr(0, 4) == "host") {
		std::string ref = ((((m_ysfNetwork == NULL) && (m_fcsNetwork == NULL)) || (m_linkType == LINK_NONE)) ? "NONE" : m_current);
		std::string host = std::string("ysf:\"") + ref + "\"";
		m_mqtt->publish("response", host);
	} else {
		CUtils::dump("Invalid remote command received", (unsigned char*)command.c_str(), command.length());
	}
}

void CYSFGateway::writeJSONStatus(const std::string& status)
{
	nlohmann::json json;

	json["timestamp"] = CUtils::createTimestamp();
	json["message"]   = status;

	WriteJSON("status", json);
}

void CYSFGateway::writeJSONLinking(const std::string& reason, const std::string& protocol, const std::string& reflector)
{
	nlohmann::json json;

	json["timestamp"] = CUtils::createTimestamp();
	json["action"]    = "linking";
	json["reason"]    = reason;
	json["reflector"] = reflector;
	json["protocol"]  = protocol;

	WriteJSON("link", json);
}

void CYSFGateway::writeJSONUnlinked(const std::string& reason)
{
	nlohmann::json json;

	json["timestamp"] = CUtils::createTimestamp();
	json["action"]    = "unlinked";
	json["reason"]    = reason;

	WriteJSON("link", json);
}

void CYSFGateway::writeJSONRelinking(const std::string& protocol, const std::string& reflector)
{
	nlohmann::json json;

	json["timestamp"] = CUtils::createTimestamp();
	json["action"]    = "relinking";
	json["reflector"] = reflector;
	json["protocol"]  = protocol;

	WriteJSON("link", json);
}

void CYSFGateway::onCommand(const unsigned char* command, unsigned int length)
{
	assert(gateway != NULL);
	assert(command != NULL);

	gateway->writeCommand(std::string((char*)command, length));
}

