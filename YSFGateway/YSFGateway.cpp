/*
*   Copyright (C) 2016-2020,2024,2025 by Jonathan Naylor G4KLX
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <clocale>
#include <cmath>
#include <algorithm>

static bool m_killed = false;
static int  m_signal = 0;

#if !defined(_WIN32) && !defined(_WIN64)
static void sigHandler(int signum)
{
	m_killed = true;
	m_signal = signum;
}
#endif

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
		m_signal = 0;
		m_killed = false;

		CYSFGateway* gateway = new CYSFGateway(std::string(iniFile));
		ret = gateway->run();

		delete gateway;

		switch (m_signal) {
			case 0:
				break;
			case 2:
				::LogInfo("YSFGateway-%s exited on receipt of SIGINT", VERSION);
				break;
			case 15:
				::LogInfo("YSFGateway-%s exited on receipt of SIGTERM", VERSION);
				break;
			case 1:
				::LogInfo("YSFGateway-%s is restarting on receipt of SIGHUP", VERSION);
				break;
			default:
				::LogInfo("YSFGateway-%s exited on receipt of an unknown signal", VERSION);
				break;
		}
	} while (m_signal == 1);

	return ret;
}

CYSFGateway::CYSFGateway(const std::string& configFile) :
m_callsign(),
m_suffix(),
m_conf(configFile),
m_writer(nullptr),
m_gps(nullptr),
m_reflectors(nullptr),
m_wiresX(nullptr),
m_dtmf(),
m_ysfNetwork(nullptr),
m_fcsNetwork(nullptr),
m_linkType(LINK_TYPE::NONE),
m_current(),
m_startup(),
m_options(),
m_exclude(false),
m_inactivityTimer(1000U),
m_lostTimer(1000U, 120U),
m_fcsNetworkEnabled(false),
m_remoteSocket(nullptr)
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
			if (user == nullptr) {
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
        ret = ::LogInitialise(m_daemon, m_conf.getLogFilePath(), m_conf.getLogFileRoot(), m_conf.getLogFileLevel(), m_conf.getLogDisplayLevel(), m_conf.getLogFileRotate());
#else
        ret = ::LogInitialise(false, m_conf.getLogFilePath(), m_conf.getLogFileRoot(), m_conf.getLogFileLevel(), m_conf.getLogDisplayLevel(), m_conf.getLogFileRotate());
#endif
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

	bool debug = m_conf.getDebug();
	sockaddr_storage rptAddr;
	unsigned int rptAddrLen;
	if (CUDPSocket::lookup(m_conf.getRptAddress(), m_conf.getRptPort(), rptAddr, rptAddrLen) != 0) {
		::fprintf(stderr, "YSFGateway: cannot find the address of the MMDVM Host");
		return 1;
	}

	std::string myAddress = m_conf.getMyAddress();
	unsigned short myPort = m_conf.getMyPort();
	CYSFNetwork rptNetwork(myAddress, myPort, m_callsign, debug);

	ret = rptNetwork.setDestination("MMDVM", rptAddr, rptAddrLen);
	if (!ret) {
		::LogError("Cannot open the repeater network port");
		::LogFinalise();
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
			::LogFinalise();
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

	if (m_conf.getRemoteCommandsEnabled()) {
		m_remoteSocket = new CUDPSocket(m_conf.getRemoteCommandsPort());
		ret = m_remoteSocket->open();
		if (!ret) {
			delete m_remoteSocket;
			m_remoteSocket = nullptr;
		}
	}

	m_startup   = m_conf.getNetworkStartup();
	m_options   = m_conf.getNetworkOptions();
	bool revert = m_conf.getNetworkRevert();
	bool wiresXCommandPassthrough = m_conf.getWiresXCommandPassthrough();

	startupLinking();

	CStopWatch stopWatch;
	stopWatch.start();

	LogMessage("YSFGateway-%s is starting", VERSION);
	LogMessage("Built %s %s (GitID #%.7s)", __TIME__, __DATE__, gitversion);

	while (!m_killed) {
		unsigned char buffer[200U];
		memset(buffer, 0U, 200U);

		while (rptNetwork.read(buffer) > 0U) {
			CYSFFICH fich;
			bool valid = fich.decode(buffer + 35U);
			m_exclude = false;
			if (valid) {
				unsigned char dt = fich.getDT();

				CYSFReflector* reflector = m_wiresX->getReflector();
				if (m_ysfNetwork != nullptr && m_linkType == LINK_TYPE::YSF && wiresXCommandPassthrough && reflector->m_wiresX) {
					processDTMF(buffer, dt);
					processWiresX(buffer, fich, true, wiresXCommandPassthrough);
				} else {
					processDTMF(buffer, dt);
					processWiresX(buffer, fich, false, wiresXCommandPassthrough);
					reflector = m_wiresX->getReflector(); //reflector may have changed
					if (m_ysfNetwork != nullptr && m_linkType == LINK_TYPE::YSF && reflector->m_wiresX)
						m_exclude = (dt == YSF_DT_DATA_FR_MODE);
				}

				if (m_gps != nullptr)
					m_gps->data(buffer + 14U, buffer + 35U, fich);
			}

			if (m_ysfNetwork != nullptr && m_linkType == LINK_TYPE::YSF && !m_exclude) {
				if (::memcmp(buffer + 0U, "YSFD", 4U) == 0) {
					m_ysfNetwork->write(buffer);
					m_inactivityTimer.start();
				}
			}

			if (m_fcsNetwork != nullptr && m_linkType == LINK_TYPE::FCS && !m_exclude) {
				if (::memcmp(buffer + 0U, "YSFD", 4U) == 0) {
					m_fcsNetwork->write(buffer);
					m_inactivityTimer.start();
				}
			}

			if ((buffer[34U] & 0x01U) == 0x01U) {
				if (m_gps != nullptr)
					m_gps->reset();
				m_dtmf.reset();
				m_exclude = false;
			}
		}

		if (m_ysfNetwork != nullptr) {
			while (m_ysfNetwork->read(buffer) > 0U) {
				if (m_linkType == LINK_TYPE::YSF) {
					// Only pass through YSF data packets
					if (::memcmp(buffer + 0U, "YSFD", 4U) == 0 && !m_wiresX->isBusy())
						rptNetwork.write(buffer);

					m_lostTimer.start();
				}
			}
		}

		if (m_fcsNetwork != nullptr) {
			while (m_fcsNetwork->read(buffer) > 0U) {
				if (m_linkType == LINK_TYPE::FCS) {
					// Only pass through YSF data packets
					if (::memcmp(buffer + 0U, "YSFD", 4U) == 0 && !m_wiresX->isBusy())
						rptNetwork.write(buffer);

					m_lostTimer.start();
				}
			}
		}

		if (m_remoteSocket != nullptr)
			processRemoteCommands();

		unsigned int ms = stopWatch.elapsed();
		stopWatch.start();

		rptNetwork.clock(ms);
		if (m_ysfNetwork != nullptr)
			m_ysfNetwork->clock(ms);
		if (m_fcsNetwork != nullptr)
			m_fcsNetwork->clock(ms);
		if (m_writer != nullptr)
			m_writer->clock(ms);
		m_wiresX->clock(ms);

		m_inactivityTimer.clock(ms);
		if (m_inactivityTimer.isRunning() && m_inactivityTimer.hasExpired()) {
			if (revert) {
				if (m_current != m_startup) {
					if (m_linkType == LINK_TYPE::YSF) {
						m_wiresX->processDisconnect();
						m_ysfNetwork->writeUnlink(3U);
						m_ysfNetwork->clearDestination();
					}

					if (m_linkType == LINK_TYPE::FCS) {
						m_fcsNetwork->writeUnlink(3U);
						m_fcsNetwork->clearDestination();
					}

					m_current.clear();
					m_lostTimer.stop();
					m_linkType = LINK_TYPE::NONE;

					startupLinking();
				}
			} else {
				if (m_linkType == LINK_TYPE::YSF) {
					LogMessage("Disconnecting due to inactivity");
					m_wiresX->processDisconnect();
					m_ysfNetwork->writeUnlink(3U);
					m_ysfNetwork->clearDestination();
				}

				if (m_linkType == LINK_TYPE::FCS) {
					LogMessage("Disconnecting due to inactivity");
					m_fcsNetwork->writeUnlink(3U);
					m_fcsNetwork->clearDestination();
				}

				m_lostTimer.stop();
				m_linkType = LINK_TYPE::NONE;
			}

			m_inactivityTimer.start();
		}

		m_lostTimer.clock(ms);
		if (m_lostTimer.isRunning() && m_lostTimer.hasExpired()) {
			if (m_linkType == LINK_TYPE::YSF) {
				LogWarning("Link has failed, polls lost");
				m_wiresX->processDisconnect();
				m_ysfNetwork->clearDestination();
			}

			if (m_fcsNetwork != nullptr) {
				LogWarning("Link has failed, polls lost");
				m_fcsNetwork->clearDestination();
			}

			m_inactivityTimer.start();
			m_lostTimer.stop();
			m_linkType = LINK_TYPE::NONE;
		}

		if (ms < 5U)
			CThread::sleep(5U);
	}

	rptNetwork.clearDestination();

	if (m_gps != nullptr) {
		m_writer->close();
		delete m_writer;
		delete m_gps;
	}

	if (m_ysfNetwork != nullptr) {
		m_ysfNetwork->clearDestination();
		delete m_ysfNetwork;
	}

	if (m_fcsNetwork != nullptr) {
		m_fcsNetwork->close();
		delete m_fcsNetwork;
	}

	if (m_remoteSocket != nullptr) {
		m_remoteSocket->close();
		delete m_remoteSocket;
	}

	delete m_wiresX;

	::LogFinalise();

	return 0;
}

void CYSFGateway::createGPS()
{
	if (!m_conf.getAPRSEnabled())
		return;

	std::string address = m_conf.getAPRSAddress();
	unsigned short port = m_conf.getAPRSPort();
	std::string suffix  = m_conf.getAPRSSuffix();
	bool debug          = m_conf.getDebug();

	m_writer = new CAPRSWriter(m_callsign, m_suffix, address, port, suffix, debug);

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
		m_writer = nullptr;
		return;
	}

	m_gps = new CGPS(m_writer);
}

void CYSFGateway::createWiresX(CYSFNetwork* rptNetwork)
{
	assert(rptNetwork != nullptr);

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

	std::string filename = m_conf.getFCSNetworkFile();
	if (m_fcsNetworkEnabled)
		readFCSRoomsFile(filename);

	m_reflectors->load();
	m_wiresX->start();
}

void CYSFGateway::processWiresX(const unsigned char* buffer, const CYSFFICH& fich, bool dontProcessWiresXLocal, bool wiresXCommandPassthrough)
{
	assert(buffer != nullptr);

	WX_STATUS status = m_wiresX->process(buffer + 35U, buffer + 14U, fich, dontProcessWiresXLocal);
	switch (status) {
	case WX_STATUS::CONNECT_YSF: {
			if (m_linkType == LINK_TYPE::YSF)
				m_ysfNetwork->writeUnlink(3U);

			if (m_linkType == LINK_TYPE::FCS) {
				m_fcsNetwork->writeUnlink(3U);
				m_fcsNetwork->clearDestination();
			}

			CYSFReflector* reflector = m_wiresX->getReflector();
			LogMessage("Connect to %5.5s - \"%s\" has been requested by %10.10s", reflector->m_id.c_str(), reflector->m_name.c_str(), buffer + 14U);

			m_ysfNetwork->setDestination(reflector->m_name, reflector->m_addr, reflector->m_addrLen);
			m_ysfNetwork->writePoll(3U);

			m_current = reflector->m_id;
			m_inactivityTimer.start();
			m_lostTimer.start();
			m_linkType = LINK_TYPE::YSF;

			// If we are linking to a YSF2xxx mode, send the YSF2xxx gateway the link command too
			if (reflector->m_wiresX && wiresXCommandPassthrough) {
				LogMessage("Forward WiresX Connect to \"%s\"", reflector->m_name.c_str());
				m_wiresX->sendConnect(m_ysfNetwork);
			}
		}
		break;
	case WX_STATUS::CONNECT_FCS: {
			if (m_linkType == LINK_TYPE::YSF) {
				m_ysfNetwork->writeUnlink(3U);
				m_ysfNetwork->clearDestination();
			}

			if (m_linkType == LINK_TYPE::FCS)
				m_fcsNetwork->writeUnlink(3U);

			m_current.clear();
			m_inactivityTimer.start();
			m_lostTimer.stop();
			m_linkType = LINK_TYPE::NONE;

			CYSFReflector* reflector = m_wiresX->getReflector();
			LogMessage("Connect to %s - \"%s\" has been requested by %10.10s", reflector->m_id.c_str(), reflector->m_name.c_str(), buffer + 14U);

			std::string name = reflector->m_name;
			name.resize(8U, '0');

			bool ok = m_fcsNetwork->writeLink(name);
			if (ok) {
				m_current = name;
				m_lostTimer.start();
				m_linkType = LINK_TYPE::FCS;
			} else {
				LogMessage("Unknown reflector - %s", name.c_str());
			}
		}
		break;
	case WX_STATUS::DISCONNECT:
		if (m_linkType == LINK_TYPE::YSF) {
			LogMessage("Disconnect has been requested by %10.10s", buffer + 14U);
			if ( (wiresXCommandPassthrough) && (::memcmp(buffer + 0U, "YSFD", 4U) == 0) ) {
				// Send the disconnect to the YSF2xxx gateway too
				m_ysfNetwork->write(buffer);
			}

			m_ysfNetwork->writeUnlink(3U);
			m_ysfNetwork->clearDestination();

			m_current.clear();
			m_inactivityTimer.start();
			m_lostTimer.stop();
			m_linkType = LINK_TYPE::NONE;
		}
		if (m_linkType == LINK_TYPE::FCS) {
			LogMessage("Disconnect has been requested by %10.10s", buffer + 14U);

			m_fcsNetwork->writeUnlink(3U);
			m_fcsNetwork->clearDestination();

			m_current.clear();
			m_inactivityTimer.start();
			m_lostTimer.stop();
			m_linkType = LINK_TYPE::NONE;
		}
		break;
	case WX_STATUS::RECONNECT_CURRENT: {
			if (m_wiresX->getReflector() == nullptr && !m_current.empty()) {
				// trying to reconnect
				if (m_current.substr(0U, 3U) == "FCS" && m_fcsNetwork != nullptr) {
					m_inactivityTimer.stop();
					m_lostTimer.stop();

					bool ok = m_fcsNetwork->writeLink(m_current);
					m_fcsNetwork->setOptions(m_options);

					if (ok) {
						LogMessage("Automatic (re-)connection to %s", m_current.c_str());

						m_inactivityTimer.start();
						m_lostTimer.start();
						m_linkType = LINK_TYPE::FCS;;
					}
				} else if (m_ysfNetwork != nullptr) {
					m_inactivityTimer.stop();
					m_lostTimer.stop();

					CYSFReflector* reflector = m_reflectors->findByName(m_current);
					if (reflector != nullptr) {
						LogMessage("Automatic (re-)connection to %5.5s - \"%s\"", reflector->m_id.c_str(), reflector->m_name.c_str());

						m_wiresX->setReflector(reflector);

						m_ysfNetwork->setDestination(reflector->m_name, reflector->m_addr, reflector->m_addrLen);
						m_ysfNetwork->setOptions(m_options);
						m_ysfNetwork->writePoll(3U);

						m_inactivityTimer.start();
						m_lostTimer.start();
						m_linkType = LINK_TYPE::YSF;
					}
				}
			}
		}
		break;
	default:
		break;
	}
}

void CYSFGateway::processDTMF(unsigned char* buffer, unsigned char dt)
{
	assert(buffer != nullptr);

	WX_STATUS status = WX_STATUS::NONE;
	switch (dt) {
	case YSF_DT_VD_MODE2:
		status = m_dtmf.decodeVDMode2(buffer + 35U, (buffer[34U] & 0x01U) == 0x01U);
		break;
	default:
		break;
	}

	switch (status) {
	case WX_STATUS::CONNECT_YSF: {
			std::string id = m_dtmf.getReflector();
			CYSFReflector* reflector = m_reflectors->findById(id);
			if (reflector != nullptr) {
				m_wiresX->processConnect(reflector);

				if (m_linkType == LINK_TYPE::YSF)
					m_ysfNetwork->writeUnlink(3U);

				if (m_linkType == LINK_TYPE::FCS) {
					m_fcsNetwork->writeUnlink(3U);
					m_fcsNetwork->clearDestination();
				}

				LogMessage("Connect via DTMF to %5.5s - \"%s\" has been requested by %10.10s", reflector->m_id.c_str(), reflector->m_name.c_str(), buffer + 14U);

				m_ysfNetwork->setDestination(reflector->m_name, reflector->m_addr, reflector->m_addrLen);
				m_ysfNetwork->writePoll(3U);

				m_current = id;
				m_inactivityTimer.start();
				m_lostTimer.start();
				m_linkType = LINK_TYPE::YSF;
			}
		}
		break;
	case WX_STATUS::CONNECT_FCS: {
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

			if (m_linkType == LINK_TYPE::YSF) {
				m_wiresX->processDisconnect();
				m_ysfNetwork->writeUnlink(3U);
				m_ysfNetwork->clearDestination();
			}
			if (m_linkType == LINK_TYPE::FCS)
				m_fcsNetwork->writeUnlink(3U);

			m_current.clear();
			m_inactivityTimer.stop();
			m_lostTimer.stop();
			m_linkType = LINK_TYPE::NONE;

			LogMessage("Connect via DTMF to %s has been requested by %10.10s", id.c_str(), buffer + 14U);

			bool ok = m_fcsNetwork->writeLink(id);
			if (ok) {
				m_current = id;
				m_inactivityTimer.start();
				m_lostTimer.start();
				m_linkType = LINK_TYPE::FCS;
			} else {
				LogMessage("Unknown reflector - %s", id.c_str());
			}
		}
		break;
	case WX_STATUS::DISCONNECT:
		if (m_linkType == LINK_TYPE::YSF) {
			m_wiresX->processDisconnect();

			LogMessage("Disconnect via DTMF has been requested by %10.10s", buffer + 14U);

			m_ysfNetwork->writeUnlink(3U);
			m_ysfNetwork->clearDestination();

			m_current.clear();
			m_inactivityTimer.start();
			m_lostTimer.stop();
			m_linkType = LINK_TYPE::NONE;
		}
		if (m_linkType == LINK_TYPE::FCS) {
			LogMessage("Disconnect via DTMF has been requested by %10.10s", buffer + 14U);

			m_fcsNetwork->writeUnlink(3U);
			m_fcsNetwork->clearDestination();

			m_current.clear();
			m_inactivityTimer.start();
			m_lostTimer.stop();
			m_linkType = LINK_TYPE::NONE;
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

void CYSFGateway::startupLinking()
{
	if (!m_startup.empty()) {
		if (m_startup.substr(0U, 3U) == "FCS" && m_fcsNetwork != nullptr) {
			m_current.clear();
			m_inactivityTimer.stop();
			m_lostTimer.stop();
			m_linkType = LINK_TYPE::NONE;

			bool ok = m_fcsNetwork->writeLink(m_startup);
			m_fcsNetwork->setOptions(m_options);

			if (ok) {
				LogMessage("Automatic (re-)connection to %s", m_startup.c_str());

				m_current = m_startup;
				m_inactivityTimer.start();
				m_lostTimer.start();
				m_linkType = LINK_TYPE::FCS;
			} else {
				LogMessage("Unknown reflector - %s", m_startup.c_str());
			}
		} else if (m_ysfNetwork != nullptr) {
			m_current.clear();
			m_inactivityTimer.stop();
			m_lostTimer.stop();
			m_linkType = LINK_TYPE::NONE;

			CYSFReflector* reflector = m_reflectors->findByName(m_startup);
			if (reflector != nullptr) {
				LogMessage("Automatic (re-)connection to %5.5s - \"%s\"", reflector->m_id.c_str(), reflector->m_name.c_str());

				m_ysfNetwork->setOptions(m_options);

				m_wiresX->setReflector(reflector);

				m_ysfNetwork->setDestination(reflector->m_name, reflector->m_addr, reflector->m_addrLen);
				m_ysfNetwork->writePoll(3U);

				m_current = m_startup;
				m_inactivityTimer.start();
				m_lostTimer.start();
				m_linkType = LINK_TYPE::YSF;
			}
		}
	}
	if (m_startup.empty())
		LogMessage("No connection startup");
}

void CYSFGateway::readFCSRoomsFile(const std::string& filename)
{
	FILE* fp = ::fopen(filename.c_str(), "rt");
	if (fp == nullptr)
		return;

	unsigned int count = 0U;

	char buffer[200U];
	while (::fgets(buffer, 200, fp) != nullptr) {
		if (buffer[0U] == '#')
			continue;

		char* p1 = ::strtok(buffer, ";");
		char* p2 = ::strtok(nullptr, ";");

		if (p1 != nullptr && p2 != nullptr) {
			m_wiresX->addFCSRoom(p1, p2);
			count++;
		}
	}

	::fclose(fp);

	LogInfo("Loaded %u FCS room descriptions", count);
}

void CYSFGateway::processRemoteCommands()
{
	unsigned char buffer[200U];
	sockaddr_storage addr;
	unsigned int addrLen;

	int res = m_remoteSocket->read(buffer, 200U, addr, addrLen);
	if (res > 0) {
		buffer[res] = '\0';
		if ((::memcmp(buffer + 0U, "LinkYSF", 7U) == 0) && (strlen((char*)buffer + 0U) > 8)) {
			std::string id = std::string((char*)(buffer + 8U));
			// Left trim
			id.erase(id.begin(), std::find_if(id.begin(), id.end(), [](unsigned char ch) { return !std::isspace(ch); }));
			CYSFReflector* reflector = m_reflectors->findById(id);
			if (reflector == nullptr)
				reflector = m_reflectors->findByName(id);
			if (reflector != nullptr) {
				m_wiresX->processConnect(reflector);

				if (m_linkType == LINK_TYPE::YSF)
					m_ysfNetwork->writeUnlink(3U);

				if (m_linkType == LINK_TYPE::FCS) {
					m_fcsNetwork->writeUnlink(3U);
					m_fcsNetwork->clearDestination();
				}

				LogMessage("Connect by remote command to %5.5s - \"%s\"", reflector->m_id.c_str(), reflector->m_name.c_str());

				m_ysfNetwork->setDestination(reflector->m_name, reflector->m_addr, reflector->m_addrLen);
				m_ysfNetwork->writePoll(3U);

				m_current = id;
				m_inactivityTimer.start();
				m_lostTimer.start();
				m_linkType = LINK_TYPE::YSF;
			} else {
				LogWarning("Invalid YSF reflector id/name - \"%s\"", id.c_str());
				return;
			}
		} else if ((::memcmp(buffer + 0U, "LinkFCS", 7U) == 0) && (strlen((char*)buffer + 0U) > 8)) {
			std::string raw = std::string((char*)(buffer + 8U));
			// Left trim
			raw.erase(raw.begin(), std::find_if(raw.begin(), raw.end(), [](unsigned char ch) { return !std::isspace(ch); }));
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

			if (m_linkType == LINK_TYPE::YSF) {
				m_wiresX->processDisconnect();
				m_ysfNetwork->writeUnlink(3U);
				m_ysfNetwork->clearDestination();
			}
			if (m_linkType == LINK_TYPE::FCS)
				m_fcsNetwork->writeUnlink(3U);

			m_current.clear();
			m_inactivityTimer.stop();
			m_lostTimer.stop();
			m_linkType = LINK_TYPE::NONE;

			LogMessage("Connect by remote command to %s", id.c_str());

			bool ok = m_fcsNetwork->writeLink(id);
			if (ok) {
				m_current = id;
				m_inactivityTimer.start();
				m_lostTimer.start();
				m_linkType = LINK_TYPE::FCS;
			} else {
				LogMessage("Unknown reflector - %s", id.c_str());
			}
		} else if (::memcmp(buffer + 0U, "UnLink", 6U) == 0) {
			if (m_linkType == LINK_TYPE::YSF) {
				m_wiresX->processDisconnect();

				LogMessage("Disconnect by remote command");

				m_ysfNetwork->writeUnlink(3U);
				m_ysfNetwork->clearDestination();

				m_current.clear();
				m_inactivityTimer.stop();
				m_lostTimer.stop();
				m_linkType = LINK_TYPE::NONE;
			}
			if (m_linkType == LINK_TYPE::FCS) {
				LogMessage("Disconnect by remote command");

				m_fcsNetwork->writeUnlink(3U);
				m_fcsNetwork->clearDestination();

				m_current.clear();
				m_inactivityTimer.stop();
				m_lostTimer.stop();
				m_linkType = LINK_TYPE::NONE;
			}
		} else if (::memcmp(buffer + 0U, "status", 6U) == 0) {
			std::string state = std::string("ysf:") + (((m_ysfNetwork == nullptr) && (m_fcsNetwork == nullptr)) ? "n/a" : ((m_linkType != LINK_TYPE::NONE) ? "conn" : "disc"));
			m_remoteSocket->write((unsigned char*)state.c_str(), (unsigned int)state.length(), addr, addrLen);
		} else if (::memcmp(buffer + 0U, "host", 4U) == 0) {
			std::string ref = ((((m_ysfNetwork == nullptr) && (m_fcsNetwork == nullptr)) || (m_linkType == LINK_TYPE::NONE)) ? "NONE" : m_current);
			std::string host = std::string("ysf:\"") + ref + "\"";
			m_remoteSocket->write((unsigned char*)host.c_str(), (unsigned int)host.length(), addr, addrLen);
		} else {
			CUtils::dump("Invalid remote command received", buffer, res);
		}
	}
}
