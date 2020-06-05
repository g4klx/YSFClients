/*
 *   Copyright (C) 2015-2020 by Jonathan Naylor G4KLX
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

#include "Conf.h"
#include "Log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

const int BUFFER_SIZE = 500;

enum SECTION {
  SECTION_NONE,
  SECTION_GENERAL,
  SECTION_INFO,
  SECTION_LOG,
  SECTION_APRS,
  SECTION_NETWORK,
  SECTION_YSF_NETWORK,
  SECTION_FCS_NETWORK,
  SECTION_GPSD,
  SECTION_REMOTE_COMMANDS
};

CConf::CConf(const std::string& file) :
m_file(file),
m_callsign(),
m_suffix(),
m_id(0U),
m_rptAddress(),
m_rptPort(0U),
m_myAddress(),
m_myPort(0U),
m_wiresXMakeUpper(true),
m_wiresXCommandPassthrough(false),
m_debug(false),
m_daemon(false),
m_rxFrequency(0U),
m_txFrequency(0U),
m_power(0U),
m_latitude(0.0F),
m_longitude(0.0F),
m_height(0),
m_name(),
m_description(),
m_logDisplayLevel(0U),
m_logFileLevel(0U),
m_logFilePath(),
m_logFileRoot(),
m_aprsEnabled(false),
m_aprsAddress(),
m_aprsPort(0U),
m_aprsSuffix(),
m_aprsDescription(),
m_networkStartup(),
m_networkInactivityTimeout(0U),
m_networkRevert(false),
m_networkDebug(false),
m_ysfNetworkEnabled(false),
m_ysfNetworkPort(0U),
m_ysfNetworkHosts(),
m_ysfNetworkReloadTime(0U),
m_ysfNetworkParrotAddress("127.0.0.1"),
m_ysfNetworkParrotPort(0U),
m_ysfNetworkYSF2DMRAddress("127.0.0.1"),
m_ysfNetworkYSF2DMRPort(0U),
m_ysfNetworkYSF2NXDNAddress("127.0.0.1"),
m_ysfNetworkYSF2NXDNPort(0U),
m_ysfNetworkYSF2P25Address("127.0.0.1"),
m_ysfNetworkYSF2P25Port(0U),
m_fcsNetworkEnabled(false),
m_fcsNetworkFile(),
m_fcsNetworkPort(0U),
m_gpsdEnabled(false),
m_gpsdAddress(),
m_gpsdPort(),
m_remoteCommandsEnabled(false),
m_remoteCommandsPort(6073U)
{
}

CConf::~CConf()
{
}

bool CConf::read()
{
  FILE* fp = ::fopen(m_file.c_str(), "rt");
  if (fp == NULL) {
    ::fprintf(stderr, "Couldn't open the .ini file - %s\n", m_file.c_str());
    return false;
  }

  SECTION section = SECTION_NONE;

  char buffer[BUFFER_SIZE];
  while (::fgets(buffer, BUFFER_SIZE, fp) != NULL) {
    if (buffer[0U] == '#')
      continue;

    if (buffer[0U] == '[') {
      if (::strncmp(buffer, "[General]", 9U) == 0)
        section = SECTION_GENERAL;
	  else if (::strncmp(buffer, "[Info]", 6U) == 0)
		  section = SECTION_INFO;
	  else if (::strncmp(buffer, "[Log]", 5U) == 0)
		  section = SECTION_LOG;
	  else if (::strncmp(buffer, "[APRS]", 6U) == 0)
		  section = SECTION_APRS;
	  else if (::strncmp(buffer, "[Network]", 9U) == 0)
		  section = SECTION_NETWORK;
	  else if (::strncmp(buffer, "[YSF Network]", 13U) == 0)
		  section = SECTION_YSF_NETWORK;
	  else if (::strncmp(buffer, "[FCS Network]", 13U) == 0)
		  section = SECTION_FCS_NETWORK;
	  else if (::strncmp(buffer, "[GPSD]", 6U) == 0)
		  section = SECTION_GPSD;
	  else if (::strncmp(buffer, "[Remote Commands]", 17U) == 0)
		  section = SECTION_REMOTE_COMMANDS;
	  else
	  	  section = SECTION_NONE;

	  continue;
    }

    char* key = ::strtok(buffer, " \t=\r\n");
    if (key == NULL)
      continue;

    char* value = ::strtok(NULL, "\r\n");
	if (section == SECTION_GENERAL) {
		if (::strcmp(key, "Callsign") == 0) {
			// Convert the callsign to upper case
			for (unsigned int i = 0U; value[i] != 0; i++)
				value[i] = ::toupper(value[i]);
			m_callsign = value;
		} else if (::strcmp(key, "Suffix") == 0) {
			// Convert the callsign to upper case
			for (unsigned int i = 0U; value[i] != 0; i++)
				value[i] = ::toupper(value[i]);
			m_suffix = value;
		} else if (::strcmp(key, "Id") == 0)
			m_id = (unsigned int)::atoi(value);
		else if (::strcmp(key, "RptAddress") == 0)
			m_rptAddress = value;
		else if (::strcmp(key, "RptPort") == 0)
			m_rptPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "LocalAddress") == 0)
			m_myAddress = value;
		else if (::strcmp(key, "LocalPort") == 0)
			m_myPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "WiresXMakeUpper") == 0)
			m_wiresXMakeUpper = ::atoi(value) == 1;
		else if (::strcmp(key, "WiresXCommandPassthrough") == 0)
			m_wiresXCommandPassthrough = ::atoi(value) == 1;
		else if (::strcmp(key, "Debug") == 0)
			m_debug = ::atoi(value) == 1;
		else if (::strcmp(key, "Daemon") == 0)
			m_daemon = ::atoi(value) == 1;
	} else if (section == SECTION_INFO) {
		if (::strcmp(key, "TXFrequency") == 0)
			m_txFrequency = (unsigned int)::atoi(value);
		else if (::strcmp(key, "RXFrequency") == 0)
			m_rxFrequency = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Power") == 0)
			m_power = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Latitude") == 0)
			m_latitude = float(::atof(value));
		else if (::strcmp(key, "Longitude") == 0)
			m_longitude = float(::atof(value));
		else if (::strcmp(key, "Height") == 0)
			m_height = ::atoi(value);
		else if (::strcmp(key, "Name") == 0)
			m_name = value;
		else if (::strcmp(key, "Description") == 0)
			m_description = value;
	} else if (section == SECTION_LOG) {
		if (::strcmp(key, "FilePath") == 0)
			m_logFilePath = value;
		else if (::strcmp(key, "FileRoot") == 0)
			m_logFileRoot = value;
		else if (::strcmp(key, "FileLevel") == 0)
			m_logFileLevel = (unsigned int)::atoi(value);
		else if (::strcmp(key, "DisplayLevel") == 0)
			m_logDisplayLevel = (unsigned int)::atoi(value);
	} else if (section == SECTION_APRS) {
		if (::strcmp(key, "Enable") == 0)
			m_aprsEnabled = ::atoi(value) == 1;
		else if (::strcmp(key, "Address") == 0)
			m_aprsAddress = value;
		else if (::strcmp(key, "Port") == 0)
			m_aprsPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Suffix") == 0)
			m_aprsSuffix = value;
		else if (::strcmp(key, "Description") == 0)
			m_aprsDescription = value;
	} else if (section == SECTION_NETWORK) {
		if (::strcmp(key, "Startup") == 0)
			m_networkStartup = value;
		else if (::strcmp(key, "InactivityTimeout") == 0)
			m_networkInactivityTimeout = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Revert") == 0)
			m_networkRevert = ::atoi(value) == 1;
		else if (::strcmp(key, "Debug") == 0)
			m_networkDebug = ::atoi(value) == 1;
	} else if (section == SECTION_YSF_NETWORK) {
		if (::strcmp(key, "Enable") == 0)
			m_ysfNetworkEnabled = ::atoi(value) == 1;
		else if (::strcmp(key, "Port") == 0)
			m_ysfNetworkPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Hosts") == 0)
			m_ysfNetworkHosts = value;
		else if (::strcmp(key, "ReloadTime") == 0)
			m_ysfNetworkReloadTime = (unsigned int)::atoi(value);
		else if (::strcmp(key, "ParrotAddress") == 0)
			m_ysfNetworkParrotAddress = value;
		else if (::strcmp(key, "ParrotPort") == 0)
			m_ysfNetworkParrotPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "YSF2DMRAddress") == 0)
			m_ysfNetworkYSF2DMRAddress = value;
		else if (::strcmp(key, "YSF2DMRPort") == 0)
			m_ysfNetworkYSF2DMRPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "YSF2NXDNAddress") == 0)
			m_ysfNetworkYSF2NXDNAddress = value;
		else if (::strcmp(key, "YSF2NXDNPort") == 0)
			m_ysfNetworkYSF2NXDNPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "YSF2P25Address") == 0)
			m_ysfNetworkYSF2P25Address = value;
		else if (::strcmp(key, "YSF2P25Port") == 0)
			m_ysfNetworkYSF2P25Port = (unsigned int)::atoi(value);
	} else if (section == SECTION_FCS_NETWORK) {
		if (::strcmp(key, "Enable") == 0)
			m_fcsNetworkEnabled = ::atoi(value) == 1;
		else if (::strcmp(key, "Rooms") == 0)
			m_fcsNetworkFile = value;
		else if (::strcmp(key, "Port") == 0)
			m_fcsNetworkPort = (unsigned int)::atoi(value);
	} else if (section == SECTION_GPSD) {
		if (::strcmp(key, "Enable") == 0)
			m_gpsdEnabled = ::atoi(value) == 1;
		else if (::strcmp(key, "Address") == 0)
			m_gpsdAddress = value;
		else if (::strcmp(key, "Port") == 0)
			m_gpsdPort = value;
	} else if (section == SECTION_REMOTE_COMMANDS) {
		if (::strcmp(key, "Enable") == 0)
			m_remoteCommandsEnabled = ::atoi(value) == 1;
		else if (::strcmp(key, "Port") == 0)
			m_remoteCommandsPort = (unsigned int)::atoi(value);
	}
  }

  ::fclose(fp);

  return true;
}

std::string CConf::getCallsign() const
{
  return m_callsign;
}

std::string CConf::getSuffix() const
{
	return m_suffix;
}

unsigned int CConf::getId() const
{
	return m_id;
}

std::string CConf::getRptAddress() const
{
	return m_rptAddress;
}

unsigned int CConf::getRptPort() const
{
	return m_rptPort;
}

std::string CConf::getMyAddress() const
{
	return m_myAddress;
}

unsigned int CConf::getMyPort() const
{
	return m_myPort;
}

bool CConf::getWiresXMakeUpper() const
{
	return m_wiresXMakeUpper;
}

bool CConf::getWiresXCommandPassthrough() const
{
	return m_wiresXCommandPassthrough;
}

bool CConf::getDebug() const
{
	return m_debug;
}

bool CConf::getDaemon() const
{
	return m_daemon;
}

unsigned int CConf::getRxFrequency() const
{
	return m_rxFrequency;
}

unsigned int CConf::getTxFrequency() const
{
	return m_txFrequency;
}

unsigned int CConf::getPower() const
{
	return m_power;
}

float CConf::getLatitude() const
{
	return m_latitude;
}

float CConf::getLongitude() const
{
	return m_longitude;
}

int CConf::getHeight() const
{
	return m_height;
}

std::string CConf::getName() const
{
	return m_name;
}

std::string CConf::getDescription() const
{
	return m_description;
}

unsigned int CConf::getLogDisplayLevel() const
{
	return m_logDisplayLevel;
}

unsigned int CConf::getLogFileLevel() const
{
	return m_logFileLevel;
}

std::string CConf::getLogFilePath() const
{
  return m_logFilePath;
}

std::string CConf::getLogFileRoot() const
{
  return m_logFileRoot;
}

bool CConf::getAPRSEnabled() const
{
	return m_aprsEnabled;
}

std::string CConf::getAPRSAddress() const
{
	return m_aprsAddress;
}

unsigned int CConf::getAPRSPort() const
{
	return m_aprsPort;
}

std::string CConf::getAPRSSuffix() const
{
	return m_aprsSuffix;
}

std::string CConf::getAPRSDescription() const
{
	return m_aprsDescription;
}

std::string CConf::getNetworkStartup() const
{
	return m_networkStartup;
}

unsigned int CConf::getNetworkInactivityTimeout() const
{
	return m_networkInactivityTimeout;
}

bool CConf::getNetworkRevert() const
{
	return m_networkRevert;
}

bool CConf::getNetworkDebug() const
{
	return m_networkDebug;
}

bool CConf::getYSFNetworkEnabled() const
{
	return m_ysfNetworkEnabled;
}

unsigned int CConf::getYSFNetworkPort() const
{
  return m_ysfNetworkPort;
}

std::string CConf::getYSFNetworkHosts() const
{
	return m_ysfNetworkHosts;
}

unsigned int CConf::getYSFNetworkReloadTime() const
{
	return m_ysfNetworkReloadTime;
}

std::string CConf::getYSFNetworkParrotAddress() const
{
	return m_ysfNetworkParrotAddress;
}

unsigned int CConf::getYSFNetworkParrotPort() const
{
	return m_ysfNetworkParrotPort;
}

std::string CConf::getYSFNetworkYSF2DMRAddress() const
{
	return m_ysfNetworkYSF2DMRAddress;
}

unsigned int CConf::getYSFNetworkYSF2DMRPort() const
{
	return m_ysfNetworkYSF2DMRPort;
}

std::string CConf::getYSFNetworkYSF2NXDNAddress() const
{
	return m_ysfNetworkYSF2NXDNAddress;
}

unsigned int CConf::getYSFNetworkYSF2NXDNPort() const
{
	return m_ysfNetworkYSF2NXDNPort;
}

std::string CConf::getYSFNetworkYSF2P25Address() const
{
	return m_ysfNetworkYSF2P25Address;
}

unsigned int CConf::getYSFNetworkYSF2P25Port() const
{
	return m_ysfNetworkYSF2P25Port;
}


bool CConf::getFCSNetworkEnabled() const
{
	return m_fcsNetworkEnabled;
}

std::string CConf::getFCSNetworkFile() const
{
	return m_fcsNetworkFile;
}

unsigned int CConf::getFCSNetworkPort() const
{
	return m_fcsNetworkPort;
}

bool CConf::getGPSDEnabled() const
{
	return m_gpsdEnabled;
}

std::string CConf::getGPSDAddress() const
{
	return m_gpsdAddress;
}

std::string CConf::getGPSDPort() const
{
	return m_gpsdPort;
}

bool CConf::getRemoteCommandsEnabled() const
{
	return m_remoteCommandsEnabled;
}

unsigned int CConf::getRemoteCommandsPort() const
{
	return m_remoteCommandsPort;
}
