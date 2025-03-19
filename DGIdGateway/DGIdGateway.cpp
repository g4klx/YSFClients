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

#include "YSFReflectors.h"
#include "DGIdGateway.h"
#include "DGIdNetwork.h"
#include "IMRSNetwork.h"
#include "YSFNetwork.h"
#include "FCSNetwork.h"
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
const char* DEFAULT_INI_FILE = "DGIdGateway.ini";
#else
const char* DEFAULT_INI_FILE = "/etc/DGIdGateway.ini";
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <clocale>
#include <cmath>

const unsigned int UNSET_DGID = 999U;

const unsigned char WIRESX_DGID = 127U;

const unsigned char DT_VD_MODE1      = 0x01U;
const unsigned char DT_VD_MODE2      = 0x02U;
const unsigned char DT_VOICE_FR_MODE = 0x04U;
const unsigned char DT_DATA_FR_MODE  = 0x08U;

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
				::fprintf(stdout, "DGIdGateway version %s git #%.7s\n", VERSION, gitversion);
				return 0;
			} else if (arg.substr(0, 1) == "-") {
				::fprintf(stderr, "Usage: DGIdGateway [-v|--version] [filename]\n");
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

		CDGIdGateway* gateway = new CDGIdGateway(std::string(iniFile));
		ret = gateway->run();

		delete gateway;

		switch (m_signal) {
			case 0:
				break;
			case 2:
				::LogInfo("DGIdGateway-%s exited on receipt of SIGINT", VERSION);
				break;
			case 15:
				::LogInfo("DGIdGateway-%s exited on receipt of SIGTERM", VERSION);
				break;
			case 1:
				::LogInfo("DGIdGateway-%s is restarting on receipt of SIGHUP", VERSION);
				break;
			default:
				::LogInfo("DGIdGateway-%s exited on receipt of an unknown signal", VERSION);
				break;
		}
	} while (m_signal == 1);

	return ret;
}

CDGIdGateway::CDGIdGateway(const std::string& configFile) :
m_callsign(),
m_suffix(),
m_conf(configFile),
m_writer(nullptr),
m_gps(nullptr)
{
	CUDPSocket::startup();
}

CDGIdGateway::~CDGIdGateway()
{
	CUDPSocket::shutdown();
}

int CDGIdGateway::run()
{
	bool ret = m_conf.read();
	if (!ret) {
		::fprintf(stderr, "DGIdGateway: cannot read the .ini file\n");
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
		::fprintf(stderr, "DGIdGateway: unable to open the log file\n");
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

	sockaddr_storage rptAddr;
	unsigned int rptAddrLen;
	if (CUDPSocket::lookup(m_conf.getRptAddress(), m_conf.getRptPort(), rptAddr, rptAddrLen) != 0) {
		LogError("Unable to resolve the address of the host");
		return 1;
	}

	bool debug            = m_conf.getDebug();
	std::string myAddress = m_conf.getMyAddress();
	unsigned short myPort   = m_conf.getMyPort();

	CYSFNetwork rptNetwork(myAddress, myPort, "MMDVM", rptAddr, rptAddrLen, m_callsign, debug);
	ret = rptNetwork.open();
	if (!ret) {
		::LogError("Cannot open the repeater network port");
		::LogFinalise();
		return 1;
	}

	rptNetwork.link();

	std::string fileName = m_conf.getYSFNetHosts();
	CYSFReflectors* reflectors = new CYSFReflectors(fileName);
	reflectors->load();

	CIMRSNetwork* imrs = new CIMRSNetwork;
	ret = imrs->open();
	if (!ret) {
		delete imrs;
		imrs = nullptr;
	}

	unsigned int currentDGId = UNSET_DGID;
	bool fromRF = false;

	CDGIdNetwork* dgIdNetwork[100U];
	for (unsigned int i = 0U; i < 100U; i++)
		dgIdNetwork[i] = nullptr; 

	std::vector<DGIdData*> dgIdData = m_conf.getDGIdData();
	for (std::vector<DGIdData*>::const_iterator it = dgIdData.begin(); it != dgIdData.end(); ++it) {
		unsigned int dgid        = (*it)->m_dgId;
		if (dgid >= 100U)
			continue;

		std::string type         = (*it)->m_type;
		bool statc               = (*it)->m_static;
		unsigned int rfHangTime  = (*it)->m_rfHangTime;
		unsigned int netHangTime = (*it)->m_netHangTime;
		bool debug               = (*it)->m_debug;
		
		if (type == "FCS") {
			std::string name         = (*it)->m_name;
			unsigned short local     = (*it)->m_local;
			unsigned int txFrequency = m_conf.getTxFrequency();
			unsigned int rxFrequency = m_conf.getRxFrequency();
			std::string locator      = calculateLocator();
			unsigned int id          = m_conf.getId();

			dgIdNetwork[dgid] = new CFCSNetwork(name, local, m_callsign, rxFrequency, txFrequency, locator, id, statc, debug);
			dgIdNetwork[dgid]->m_modes       = DT_VD_MODE1 | DT_VD_MODE2 | DT_VOICE_FR_MODE | DT_DATA_FR_MODE;
			dgIdNetwork[dgid]->m_static      = statc;
			dgIdNetwork[dgid]->m_rfHangTime  = rfHangTime;
			dgIdNetwork[dgid]->m_netHangTime = netHangTime;

			LogMessage("Added FCS:%s to DG-ID %u%s", name.c_str(), dgid, statc ? " (Static)" : "");
		} else if (type == "YSF") {
			std::string name    = (*it)->m_name;
			unsigned int local  = (*it)->m_local;

			CYSFReflector* reflector = reflectors->findByName(name);
			if (reflector != nullptr) {
				dgIdNetwork[dgid] = new CYSFNetwork(local, reflector->m_name, reflector->m_addr, reflector->m_addrLen, m_callsign, statc, debug);
				dgIdNetwork[dgid]->m_modes       = DT_VD_MODE1 | DT_VD_MODE2 | DT_VOICE_FR_MODE | DT_DATA_FR_MODE;
				dgIdNetwork[dgid]->m_static      = statc;
				dgIdNetwork[dgid]->m_rfHangTime  = rfHangTime;
				dgIdNetwork[dgid]->m_netHangTime = netHangTime;

				LogMessage("Added YSF:%s to DG-ID %u%s", name.c_str(), dgid, statc ? " (Static)" : "");
			} else {
				LogWarning("Unknown YSF reflector: %s", name.c_str());
			}
		} else if (type == "IMRS") {
			if (imrs != nullptr) {
				std::vector<IMRSDestination*> destinations = (*it)->m_destinations;
				std::vector<IMRSDest*> dests;
				std::string name = (*it)->m_name;

				for (std::vector<IMRSDestination*>::const_iterator it = destinations.begin(); it != destinations.end(); ++it) {
					sockaddr_storage addr;
					unsigned int addrLen;
					if (CUDPSocket::lookup((*it)->m_address, IMRS_PORT, addr, addrLen) == 0) {
						IMRSDest* dest = new IMRSDest;
						dest->m_dgId    = (*it)->m_dgId;
						dest->m_addr    = addr;
						dest->m_addrLen = addrLen;
						dests.push_back(dest);
					} else {
						LogWarning("Unable to resolve the address for %s", (*it)->m_address.c_str());
					}
				}

				imrs->addDGId(dgid, name, dests, debug);

				dgIdNetwork[dgid] = imrs;
				dgIdNetwork[dgid]->m_modes       = DT_VD_MODE1 | DT_VD_MODE2 | DT_VOICE_FR_MODE | DT_DATA_FR_MODE;
				dgIdNetwork[dgid]->m_static      = true;
				dgIdNetwork[dgid]->m_rfHangTime  = rfHangTime;
				dgIdNetwork[dgid]->m_netHangTime = netHangTime;

				LogMessage("Added IMRS:%s to DG-ID %u%s", name.c_str(), dgid, statc ? " (Static)" : "");
			}
		} else if (type == "Gateway") {
			unsigned short local = (*it)->m_local;

			sockaddr_storage addr;
			unsigned int     addrLen;
			if (CUDPSocket::lookup((*it)->m_address, (*it)->m_port, addr, addrLen) == 0) {
				dgIdNetwork[dgid] = new CYSFNetwork(local, "YSFGateway", addr, addrLen, m_callsign, statc, debug);
				dgIdNetwork[dgid]->m_modes       = DT_VD_MODE1 | DT_VD_MODE2 | DT_VOICE_FR_MODE | DT_DATA_FR_MODE;
				dgIdNetwork[dgid]->m_static      = statc;
				dgIdNetwork[dgid]->m_rfHangTime  = rfHangTime;
				dgIdNetwork[dgid]->m_netHangTime = netHangTime;

				LogMessage("Added YSF Gateway to DG-ID %u%s", dgid, statc ? " (Static)" : "");
			} else {
				LogWarning("Unable to resolve the address for the YSF Gateway");
			}
		} else if (type == "Parrot") {
			unsigned short local = (*it)->m_local;

			sockaddr_storage addr;
			unsigned int     addrLen;
			if (CUDPSocket::lookup((*it)->m_address, (*it)->m_port, addr, addrLen) == 0) {
				dgIdNetwork[dgid] = new CYSFNetwork(local, "PARROT", addr, addrLen, m_callsign, statc, debug);
				dgIdNetwork[dgid]->m_modes       = DT_VD_MODE1 | DT_VD_MODE2 | DT_VOICE_FR_MODE | DT_DATA_FR_MODE;
				dgIdNetwork[dgid]->m_static      = statc;
				dgIdNetwork[dgid]->m_rfHangTime  = rfHangTime;
				dgIdNetwork[dgid]->m_netHangTime = netHangTime;

				LogMessage("Added Parrot to DG-ID %u%s", dgid, statc ? " (Static)" : "");
			} else {
				LogWarning("Unable to resolve the address for the YSF Parrot");
			}
		} else if (type == "YSF2DMR") {
			unsigned short local = (*it)->m_local;

			sockaddr_storage addr;
			unsigned int     addrLen;
			if (CUDPSocket::lookup((*it)->m_address, (*it)->m_port, addr, addrLen) == 0) {
				dgIdNetwork[dgid] = new CYSFNetwork(local, "YSF2DMR", addr, addrLen, m_callsign, statc, debug);
				dgIdNetwork[dgid]->m_modes       = DT_VD_MODE1 | DT_VD_MODE2;
				dgIdNetwork[dgid]->m_static      = statc;
				dgIdNetwork[dgid]->m_rfHangTime  = rfHangTime;
				dgIdNetwork[dgid]->m_netHangTime = netHangTime;

				LogMessage("Added YSF2DMR to DG-ID %u%s", dgid, statc ? " (Static)" : "");
			} else {
				LogWarning("Unable to resolve the address for YSF2DMR");
			}
		} else if (type == "YSF2NXDN") {
			unsigned short local = (*it)->m_local;

			sockaddr_storage addr;
			unsigned int     addrLen;
			if (CUDPSocket::lookup((*it)->m_address, (*it)->m_port, addr, addrLen) == 0) {
				dgIdNetwork[dgid] = new CYSFNetwork(local, "YSF2NXDN", addr, addrLen, m_callsign, statc, debug);
				dgIdNetwork[dgid]->m_modes       = DT_VD_MODE1 | DT_VD_MODE2;
				dgIdNetwork[dgid]->m_static      = statc;
				dgIdNetwork[dgid]->m_rfHangTime  = rfHangTime;
				dgIdNetwork[dgid]->m_netHangTime = netHangTime;

				LogMessage("Added YSF2NXDN to DG-ID %u%s", dgid, statc ? " (Static)" : "");
			} else {
				LogWarning("Unable to resolve the address for YSF2NXDN");
			}
		} else if (type == "YSF2P25") {
			unsigned short local = (*it)->m_local;

			sockaddr_storage addr;
			unsigned int     addrLen;
			if (CUDPSocket::lookup((*it)->m_address, (*it)->m_port, addr, addrLen) == 0) {
				dgIdNetwork[dgid] = new CYSFNetwork(local, "YSF2P25", addr, addrLen, m_callsign, statc, debug);
				dgIdNetwork[dgid]->m_modes       = DT_VOICE_FR_MODE;
				dgIdNetwork[dgid]->m_static      = statc;
				dgIdNetwork[dgid]->m_rfHangTime  = rfHangTime;
				dgIdNetwork[dgid]->m_netHangTime = netHangTime;

				LogMessage("Added YSF2P25 to DG-ID %u%s", dgid, statc ? " (Static)" : "");
			} else {
				LogWarning("Unable to resolve the address for YSF2P25");
			}
		}
		
		if (dgIdNetwork[dgid] != nullptr && dgIdNetwork[dgid] != imrs) {
			bool ret = dgIdNetwork[dgid]->open();
			if (!ret) {
				LogWarning("\tUnable to open connection");
				delete dgIdNetwork[dgid];
				dgIdNetwork[dgid] = nullptr;
			} else if (dgIdNetwork[dgid]->m_static) {
				LogMessage("\tLinking at startup");
				dgIdNetwork[dgid]->link();
				dgIdNetwork[dgid]->link();
				dgIdNetwork[dgid]->link();
			}
		}
	}

	createGPS();

	CTimer inactivityTimer(1000U);
	CTimer bleepTimer(1000U, 1U);

	CStopWatch stopWatch;
	stopWatch.start();

	LogMessage("DGIdGateway-%s is starting", VERSION);
 	LogMessage("Built %s %s (GitID #%.7s)", __TIME__, __DATE__, gitversion);

	DGID_STATUS state = DGID_STATUS::NOTLINKED;
	unsigned int nPips = 0U;

	while (!m_killed) {
		unsigned char buffer[200U];
		memset(buffer, 0U, 200U);

		if (rptNetwork.read(0U, buffer) > 0U) {
			CYSFFICH fich;
			bool valid = fich.decode(buffer + 35U);
			if (valid) {
				unsigned char dgId = fich.getDGId();

				if (dgId == WIRESX_DGID)
					dgId = 0U;

				if (currentDGId == UNSET_DGID) {
					if (dgIdNetwork[dgId] != nullptr && !dgIdNetwork[dgId]->m_static) {
						dgIdNetwork[dgId]->link();
						dgIdNetwork[dgId]->link();
						dgIdNetwork[dgId]->link();
					}

					if (dgIdNetwork[dgId] != nullptr) {
						std::string desc = dgIdNetwork[dgId]->getDesc(dgId);
						LogMessage("DG-ID set to %u (%s) via RF", dgId, desc.c_str());
						currentDGId = dgId;
						state = DGID_STATUS::NOTLINKED;
					} else {
						LogMessage("DG-ID set to %u (None) via RF", dgId);
						state = DGID_STATUS::NOTOPEN;
					}

					fromRF = true;
				}

				if (m_gps != nullptr)
					m_gps->data(buffer + 14U, buffer + 35U, fich);

				if (currentDGId != UNSET_DGID && dgIdNetwork[currentDGId] != nullptr) {
					// Only allow the wanted modes through
					unsigned char dt = fich.getDT();
					if ((dt == YSF_DT_VD_MODE1      && (dgIdNetwork[currentDGId]->m_modes & DT_VD_MODE1) != 0U) ||
						(dt == YSF_DT_DATA_FR_MODE  && (dgIdNetwork[currentDGId]->m_modes & DT_DATA_FR_MODE) != 0U) ||
						(dt == YSF_DT_VD_MODE2      && (dgIdNetwork[currentDGId]->m_modes & DT_VD_MODE2) != 0U) ||
						(dt == YSF_DT_VOICE_FR_MODE && (dgIdNetwork[currentDGId]->m_modes & DT_VOICE_FR_MODE) != 0U)) {
						unsigned char origDGId = fich.getDGId();
						if (origDGId != WIRESX_DGID) {
							unsigned int newDGId = dgIdNetwork[currentDGId]->getDGId();
							fich.setDGId(newDGId);
							fich.encode(buffer + 35U);
						}

						dgIdNetwork[currentDGId]->write(currentDGId, buffer);
					}

					inactivityTimer.setTimeout(dgIdNetwork[currentDGId]->m_rfHangTime);
					inactivityTimer.start();
				}
			}

			if ((buffer[34U] & 0x01U) == 0x01U) {
				if (m_gps != nullptr)
					m_gps->reset();
				if (nPips > 0U && fromRF)
					bleepTimer.start();
			}
		}

		for (unsigned int i = 0U; i < 100U; i++) {
			if (dgIdNetwork[i] != nullptr) {
				unsigned int len = dgIdNetwork[i]->read(i, buffer);
				if (len > 0U && (i == currentDGId || currentDGId == UNSET_DGID)) {
					CYSFFICH fich;
					bool valid = fich.decode(buffer + 35U);
					if (valid) {
						unsigned char dgId = fich.getDGId();
						if (dgId != WIRESX_DGID) {
							fich.setDGId(i);
							fich.encode(buffer + 35U);
						}

						rptNetwork.write(0U, buffer);

						inactivityTimer.setTimeout(dgIdNetwork[i]->m_netHangTime);
						inactivityTimer.start();

						if (currentDGId == UNSET_DGID) {
							std::string desc = dgIdNetwork[i]->getDesc(i);
							LogMessage("DG-ID set to %u (%s) via Network", i, desc.c_str());
							currentDGId = i;
							state = DGID_STATUS::LINKED;
							fromRF = false;
						}
					}
				}
			}
		}

		unsigned int ms = stopWatch.elapsed();
		stopWatch.start();

		rptNetwork.clock(ms);

		for (unsigned int i = 0U; i < 100U; i++) {
			if (dgIdNetwork[i] != nullptr)
				dgIdNetwork[i]->clock(ms);
		}

		if (m_writer != nullptr)
			m_writer->clock(ms);

		inactivityTimer.clock(ms);
		if (inactivityTimer.isRunning() && inactivityTimer.hasExpired()) {
			if (dgIdNetwork[currentDGId] != nullptr && !dgIdNetwork[currentDGId]->m_static) {
				dgIdNetwork[currentDGId]->unlink();
				dgIdNetwork[currentDGId]->unlink();
				dgIdNetwork[currentDGId]->unlink();
			}

			LogMessage("DG-ID set to None via timeout");

			state = DGID_STATUS::NOTLINKED;
			currentDGId = UNSET_DGID;
			inactivityTimer.stop();

			if (fromRF) {
				sendPips(2U);
				fromRF = false;
			}
		}

		bleepTimer.clock(ms);
		if (bleepTimer.isRunning() && bleepTimer.hasExpired()) {
			sendPips(nPips);
			bleepTimer.stop();
			nPips = 0U;
		}

		if (currentDGId != UNSET_DGID && dgIdNetwork[currentDGId] != nullptr) {
			DGID_STATUS netState = dgIdNetwork[currentDGId]->getStatus();
			bool statc = dgIdNetwork[currentDGId]->m_static;
			if (fromRF && state != DGID_STATUS::LINKED && netState != DGID_STATUS::LINKED && statc)
				nPips = 3U;
			else if (fromRF && state != DGID_STATUS::LINKED && netState == DGID_STATUS::LINKED)
				nPips = 1U;
			else if (fromRF && state == DGID_STATUS::LINKED && netState != DGID_STATUS::LINKED)
				nPips = 3U;
			state = netState;
		} else {
			if (fromRF && state != DGID_STATUS::NOTLINKED)
				nPips = 2U;
			state = DGID_STATUS::NOTLINKED;
		}

		if (ms < 5U)
			CThread::sleep(5U);
	}

	rptNetwork.unlink();
	rptNetwork.close();

	if (m_gps != nullptr) {
		m_writer->close();
		delete m_writer;
		delete m_gps;
	}

	for (unsigned int i = 0U; i < 100U; i++) {
		if (dgIdNetwork[i] != nullptr && dgIdNetwork[i] != imrs) {
			dgIdNetwork[i]->unlink();
			dgIdNetwork[i]->unlink();
			dgIdNetwork[i]->unlink();
			dgIdNetwork[i]->close();
			delete dgIdNetwork[i];
		}
	}

	if (imrs != nullptr) {
		imrs->close();
		delete imrs;
	}

	::LogFinalise();

	return 0;
}

void CDGIdGateway::createGPS()
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
	std::string symbol  = m_conf.getAPRSSymbol();

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

std::string CDGIdGateway::calculateLocator()
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

void CDGIdGateway::sendPips(unsigned int n)
{
	if (n == 0U)
		return;

	bool bleep = m_conf.getBleep();
	if (bleep)
		LogMessage("*** %u bleep!", n);
}
