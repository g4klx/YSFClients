/*
 *   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
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
  SECTION_APRS_FI,
  SECTION_NETWORK
};

CConf::CConf(const std::string& file) :
m_file(file),
m_callsign(),
m_suffix(),
m_rptAddress(),
m_rptPort(0U),
m_myAddress(),
m_myPort(0U),
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
m_aprsServer(),
m_aprsPort(0U),
m_aprsPassword(),
m_networkEnabled(false),
m_networkPort(0U),
m_networkHosts(),
m_networkReloadTime(0U),
m_networkParrotAddress("127.0.0.1"),
m_networkParrotPort(0U),
m_networkStartup(),
m_networkInactivityTimeout(0U),
m_networkRevert(false),
m_networkDebug(false)
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
	  else if (::strncmp(buffer, "[aprs.fi]", 5U) == 0)
		  section = SECTION_APRS_FI;
	  else if (::strncmp(buffer, "[Network]", 5U) == 0)
		  section = SECTION_NETWORK;
	  else
        section = SECTION_NONE;

      continue;
    }

    char* key   = ::strtok(buffer, " \t=\r\n");
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
		} else if (::strcmp(key, "RptAddress") == 0)
			m_rptAddress = value;
		else if (::strcmp(key, "RptPort") == 0)
			m_rptPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "LocalAddress") == 0)
			m_myAddress = value;
		else if (::strcmp(key, "LocalPort") == 0)
			m_myPort = (unsigned int)::atoi(value);
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
	} else if (section == SECTION_APRS_FI) {
		if (::strcmp(key, "Enable") == 0)
			m_aprsEnabled = ::atoi(value) == 1;
		else if (::strcmp(key, "Server") == 0)
			m_aprsServer = value;
		else if (::strcmp(key, "Port") == 0)
			m_aprsPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Password") == 0)
			m_aprsPassword = value;
	} else if (section == SECTION_NETWORK) {
		if (::strcmp(key, "Enable") == 0)
			m_networkEnabled = ::atoi(value) == 1;
		else if (::strcmp(key, "Port") == 0)
			m_networkPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Hosts") == 0)
			m_networkHosts = value;
		else if (::strcmp(key, "ReloadTime") == 0)
			m_networkReloadTime = (unsigned int)::atoi(value);
		else if (::strcmp(key, "ParrotAddress") == 0)
			m_networkParrotAddress = value;
		else if (::strcmp(key, "ParrotPort") == 0)
			m_networkParrotPort = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Startup") == 0)
			m_networkStartup = value;
		else if (::strcmp(key, "InactivityTimeout") == 0)
			m_networkInactivityTimeout = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Revert") == 0)
			m_networkRevert = ::atoi(value) == 1;
		else if (::strcmp(key, "Debug") == 0)
			m_networkDebug = ::atoi(value) == 1;
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

std::string CConf::getAPRSServer() const
{
	return m_aprsServer;
}

unsigned int CConf::getAPRSPort() const
{
	return m_aprsPort;
}

std::string CConf::getAPRSPassword() const
{
	return m_aprsPassword;
}

bool CConf::getNetworkEnabled() const
{
	return m_networkEnabled;
}

unsigned int CConf::getNetworkPort() const
{
  return m_networkPort;
}

std::string CConf::getNetworkHosts() const
{
	return m_networkHosts;
}

unsigned int CConf::getNetworkReloadTime() const
{
	return m_networkReloadTime;
}

std::string CConf::getNetworkParrotAddress() const
{
	return m_networkParrotAddress;
}

unsigned int CConf::getNetworkParrotPort() const
{
	return m_networkParrotPort;
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
