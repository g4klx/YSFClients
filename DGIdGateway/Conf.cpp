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
#include <cassert>

const int BUFFER_SIZE = 500;

enum SECTION {
  SECTION_NONE,
  SECTION_GENERAL,
  SECTION_INFO,
  SECTION_LOG,
  SECTION_APRS,
  SECTION_YSF_NETWORK,
  SECTION_FCS_NETWORK,
  SECTION_IMRS_NETWORK,
  SECTION_DGID,
  SECTION_GPSD
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
m_rfHangTime(60U),
m_netHangTime(60U),
m_bleep(true),
m_debug(false),
m_daemon(false),
m_rxFrequency(0U),
m_txFrequency(0U),
m_power(0U),
m_latitude(0.0F),
m_longitude(0.0F),
m_height(0),
m_description(),
m_logDisplayLevel(0U),
m_logFileLevel(0U),
m_logFilePath(),
m_logFileRoot(),
m_logFileRotate(true),
m_aprsEnabled(false),
m_aprsAddress(),
m_aprsPort(0U),
m_aprsSuffix(),
m_aprsDescription(),
m_aprsSymbol("/r"),
m_ysfNetHosts(),
m_ysfRFHangTime(60U),
m_ysfNetHangTime(60U),
m_ysfNetDebug(false),
m_fcsRFHangTime(60U),
m_fcsNetHangTime(60U),
m_fcsNetDebug(false),
m_imrsRFHangTime(240U),
m_imrsNetHangTime(240U),
m_imrsNetDebug(false),
m_dgIdData(),
m_gpsdEnabled(false),
m_gpsdAddress(),
m_gpsdPort()
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

  DGIdData* dgIdData = NULL;

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
	  else if (::strncmp(buffer, "[YSF Network]", 13U) == 0)
		  section = SECTION_YSF_NETWORK;
	  else if (::strncmp(buffer, "[FCS Network]", 13U) == 0)
		  section = SECTION_FCS_NETWORK;
	  else if (::strncmp(buffer, "[IMRS Network]", 14U) == 0)
		  section = SECTION_IMRS_NETWORK;
	  else if (::strncmp(buffer, "[DGId=", 6U) == 0) {
		  section = SECTION_DGID;
		  dgIdData = new DGIdData;
		  dgIdData->m_dgId = (unsigned int)::atoi(buffer + 6U);
		  m_dgIdData.push_back(dgIdData);
	  } else if (::strncmp(buffer, "[GPSD]", 6U) == 0)
		  section = SECTION_GPSD;
	  else
	  	  section = SECTION_NONE;

	  continue;
	}

	char* key = ::strtok(buffer, " \t=\r\n");
	if (key == NULL)
		continue;

	char* value = ::strtok(NULL, "\r\n");
	if (value == NULL)
		continue;

	// Remove quotes from the value
	size_t len = ::strlen(value);
	if (len > 1U && *value == '"' && value[len - 1U] == '"') {
		value[len - 1U] = '\0';
		value++;
	} else {
		char *p;

		// if value is not quoted, remove after # (to make comment)
		if ((p = strchr(value, '#')) != NULL)
			*p = '\0';

		// remove trailing tab/space
		for (p = value + strlen(value) - 1U; p >= value && (*p == '\t' || *p == ' '); p--)
			*p = '\0';
	}

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
			m_rptPort = (unsigned short)::atoi(value);
		else if (::strcmp(key, "LocalAddress") == 0)
			m_myAddress = value;
		else if (::strcmp(key, "LocalPort") == 0)
			m_myPort = (unsigned short)::atoi(value);
		else if (::strcmp(key, "RFHangTime") == 0)
			m_ysfRFHangTime = m_fcsRFHangTime = m_rfHangTime = (unsigned int)::atoi(value);
		else if (::strcmp(key, "NetHangTime") == 0)
			m_ysfNetHangTime = m_fcsNetHangTime = m_netHangTime = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Bleep") == 0)
			m_bleep = ::atoi(value) == 1;
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
		else if (::strcmp(key, "FileRotate") == 0)
			m_logFileRotate = ::atoi(value) == 1;
	} else if (section == SECTION_APRS) {
		if (::strcmp(key, "Enable") == 0)
			m_aprsEnabled = ::atoi(value) == 1;
		else if (::strcmp(key, "Address") == 0)
			m_aprsAddress = value;
		else if (::strcmp(key, "Port") == 0)
			m_aprsPort = (unsigned short)::atoi(value);
		else if (::strcmp(key, "Suffix") == 0)
			m_aprsSuffix = value;
		else if (::strcmp(key, "Description") == 0)
			m_aprsDescription = value;
                else if (::strcmp(key, "Symbol") == 0)
                        m_aprsSymbol = value;
	} else if (section == SECTION_YSF_NETWORK) {
		if (::strcmp(key, "Hosts") == 0)
			m_ysfNetHosts = value;
		else if (::strcmp(key, "RFHangTime") == 0)
			m_ysfRFHangTime = (unsigned int)::atoi(value);
		else if (::strcmp(key, "NetHangTime") == 0)
			m_ysfNetHangTime = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Debug") == 0)
			m_ysfNetDebug = ::atoi(value) == 1;
	} else if (section == SECTION_FCS_NETWORK) {
		if (::strcmp(key, "RFHangTime") == 0)
			m_fcsRFHangTime = (unsigned int)::atoi(value);
		else if (::strcmp(key, "NetHangTime") == 0)
			m_fcsNetHangTime = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Debug") == 0)
			m_fcsNetDebug = ::atoi(value) == 1;
	} else if (section == SECTION_IMRS_NETWORK) {
		if (::strcmp(key, "RFHangTime") == 0)
			m_imrsRFHangTime = (unsigned int)::atoi(value);
		else if (::strcmp(key, "NetHangTime") == 0)
			m_imrsNetHangTime = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Debug") == 0)
			m_imrsNetDebug = ::atoi(value) == 1;
	} else if (section == SECTION_DGID) {
		assert(dgIdData != NULL);
		if (::strcmp(key, "Type") == 0) {
			dgIdData->m_type = value;
			dgIdData->m_static = false;
			if (::strcmp(value, "YSF") == 0) {
				dgIdData->m_rfHangTime  = m_ysfRFHangTime;
				dgIdData->m_netHangTime = m_ysfNetHangTime;
				dgIdData->m_debug       = m_ysfNetDebug;
			} else if (::strcmp(value, "FCS") == 0) {
				dgIdData->m_rfHangTime  = m_fcsRFHangTime;
				dgIdData->m_netHangTime = m_fcsNetHangTime;
				dgIdData->m_debug       = m_fcsNetDebug;
			} else if (::strcmp(value, "IMRS") == 0) {
				dgIdData->m_rfHangTime  = m_imrsRFHangTime;
				dgIdData->m_netHangTime = m_imrsNetHangTime;
				dgIdData->m_debug       = m_imrsNetDebug;
			} else {
				dgIdData->m_rfHangTime  = m_rfHangTime;
				dgIdData->m_netHangTime = m_netHangTime;
				dgIdData->m_debug       = false;
			}
		} else if (::strcmp(key, "RFHangTime") == 0)
			dgIdData->m_rfHangTime = (unsigned int)::atoi(value);
		else if (::strcmp(key, "NetHangTime") == 0)
			dgIdData->m_netHangTime = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Static") == 0)
			dgIdData->m_static = ::atoi(value) == 1;
		else if (::strcmp(key, "Address") == 0)
			dgIdData->m_address = value;
		else if (::strcmp(key, "Name") == 0)
			dgIdData->m_name = value;
		else if (::strcmp(key, "Port") == 0)
			dgIdData->m_port = (unsigned short)::atoi(value);
		else if (::strcmp(key, "Local") == 0)
			dgIdData->m_local = (unsigned int)::atoi(value);
		else if (::strcmp(key, "DGId") == 0)
			dgIdData->m_netDGId = (unsigned int)::atoi(value);
		else if (::strcmp(key, "Destination") == 0) {
			char* p1 = ::strtok(value, ",");
			char* p2 = ::strtok(NULL, "\r\n");
			IMRSDestination* dest = new IMRSDestination;
			dest->m_dgId    = (unsigned int)::atoi(p1);
			dest->m_address = p2;
			dgIdData->m_destinations.push_back(dest);
		} else if (::strcmp(key, "Debug") == 0)
			dgIdData->m_debug = ::atoi(value) == 1;
	} else if (section == SECTION_GPSD) {
		if (::strcmp(key, "Enable") == 0)
			m_gpsdEnabled = ::atoi(value) == 1;
		else if (::strcmp(key, "Address") == 0)
			m_gpsdAddress = value;
		else if (::strcmp(key, "Port") == 0)
			m_gpsdPort = value;
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

unsigned short CConf::getRptPort() const
{
	return m_rptPort;
}

std::string CConf::getMyAddress() const
{
	return m_myAddress;
}

unsigned short CConf::getMyPort() const
{
	return m_myPort;
}

bool CConf::getBleep() const
{
	return m_bleep;
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

bool CConf::getLogFileRotate() const
{
	return m_logFileRotate;
}

bool CConf::getAPRSEnabled() const
{
	return m_aprsEnabled;
}

std::string CConf::getAPRSAddress() const
{
	return m_aprsAddress;
}

unsigned short CConf::getAPRSPort() const
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

std::string CConf::getAPRSSymbol() const
{
       return m_aprsSymbol;
}

std::string CConf::getYSFNetHosts() const
{
	return m_ysfNetHosts;
}

std::vector<DGIdData*> CConf::getDGIdData() const
{
	return m_dgIdData;
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

