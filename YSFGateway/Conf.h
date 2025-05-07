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

#if !defined(CONF_H)
#define	CONF_H

#include <string>

class CConf
{
public:
  CConf(const std::string& file);
  ~CConf();

  bool read();

  // The General section
  std::string  getCallsign() const;
  std::string  getSuffix() const;
  unsigned int getId() const;
  std::string  getRptAddress() const;
  unsigned short getRptPort() const;
  std::string  getMyAddress() const;
  unsigned short getMyPort() const;
  bool         getWiresXMakeUpper() const;
  bool         getWiresXCommandPassthrough() const;
  bool         getDebug() const;
  bool         getDaemon() const;

  // The Info section
  unsigned int getRxFrequency() const;
  unsigned int getTxFrequency() const;
  unsigned int getPower() const;
  float        getLatitude() const;
  float        getLongitude() const;
  int          getHeight() const;
  std::string  getName() const;
  std::string  getDescription() const;

  // The Log section
  unsigned int getLogDisplayLevel() const;
  unsigned int getLogFileLevel() const;
  std::string  getLogFilePath() const;
  std::string  getLogFileRoot() const;
  bool         getLogFileRotate() const;

  // The APRS section
  bool         getAPRSEnabled() const;
  std::string  getAPRSAddress() const;
  unsigned short getAPRSPort() const;
  std::string  getAPRSSuffix() const;
  std::string  getAPRSDescription() const;
  std::string  getAPRSSymbol() const;

  // The Network section
  std::string  getNetworkStartup() const;
  std::string  getNetworkOptions() const;
  unsigned int getNetworkInactivityTimeout() const;
  bool         getNetworkReconnect() const;
  bool         getNetworkRevert() const;
  bool         getNetworkDebug() const;

  // The YSF Network section
  bool         getYSFNetworkEnabled() const;
  unsigned short getYSFNetworkPort() const;
  std::string  getYSFNetworkHosts() const;
  unsigned int getYSFNetworkReloadTime() const;
  std::string  getYSFNetworkParrotAddress() const;
  unsigned short getYSFNetworkParrotPort() const;
  std::string  getYSFNetworkYSF2DMRAddress() const;
  unsigned short getYSFNetworkYSF2DMRPort() const;
  std::string  getYSFNetworkYSF2NXDNAddress() const;
  unsigned short getYSFNetworkYSF2NXDNPort() const;
  std::string  getYSFNetworkYSF2P25Address() const;
  unsigned short getYSFNetworkYSF2P25Port() const;

  // The FCS Network section
  bool         getFCSNetworkEnabled() const;
  std::string  getFCSNetworkFile() const;
  unsigned short getFCSNetworkPort() const;

  // The GPSD section
  bool         getGPSDEnabled() const;
  std::string  getGPSDAddress() const;
  std::string  getGPSDPort() const;

  // The Remote Commands section
  bool         getRemoteCommandsEnabled() const;
  unsigned short getRemoteCommandsPort() const;

private:
  std::string  m_file;
  std::string  m_callsign;
  std::string  m_suffix;
  unsigned int m_id;
  std::string  m_rptAddress;
  unsigned short m_rptPort;
  std::string  m_myAddress;
  unsigned short m_myPort;
  bool         m_wiresXMakeUpper;
  bool         m_wiresXCommandPassthrough;
  bool         m_debug;
  bool         m_daemon;

  unsigned int m_rxFrequency;
  unsigned int m_txFrequency;
  unsigned int m_power;
  float        m_latitude;
  float        m_longitude;
  int          m_height;
  std::string  m_name;
  std::string  m_description;

  unsigned int m_logDisplayLevel;
  unsigned int m_logFileLevel;
  std::string  m_logFilePath;
  std::string  m_logFileRoot;
  bool         m_logFileRotate;

  bool         m_aprsEnabled;
  std::string  m_aprsAddress;
  unsigned short m_aprsPort;
  std::string  m_aprsSuffix;
  std::string  m_aprsDescription;
  std::string  m_aprsSymbol;

  std::string  m_networkStartup;
  std::string  m_networkOptions;
  unsigned int m_networkInactivityTimeout;
  bool         m_networkReconnect;
  bool         m_networkRevert;
  bool         m_networkDebug;

  bool         m_ysfNetworkEnabled;
  unsigned short m_ysfNetworkPort;
  std::string  m_ysfNetworkHosts;
  unsigned int m_ysfNetworkReloadTime;
  std::string  m_ysfNetworkParrotAddress;
  unsigned short m_ysfNetworkParrotPort;
  std::string  m_ysfNetworkYSF2DMRAddress;
  unsigned short m_ysfNetworkYSF2DMRPort;
  std::string  m_ysfNetworkYSF2NXDNAddress;
  unsigned short m_ysfNetworkYSF2NXDNPort;
  std::string  m_ysfNetworkYSF2P25Address;
  unsigned short m_ysfNetworkYSF2P25Port;

  bool         m_fcsNetworkEnabled;
  std::string  m_fcsNetworkFile;
  unsigned short m_fcsNetworkPort;

  bool         m_gpsdEnabled;
  std::string  m_gpsdAddress;
  std::string  m_gpsdPort;

  bool         m_remoteCommandsEnabled;
  unsigned short m_remoteCommandsPort;
};

#endif
