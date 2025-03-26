/*
 *   Copyright (C) 2015-2020,2023,2025 by Jonathan Naylor G4KLX
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
#include <vector>

struct IMRSDestination {
	std::string  m_address;
	unsigned int m_dgId;
};

struct DGIdData {
	unsigned int m_dgId;
	std::string  m_type;
	bool         m_static;
	std::string  m_name;
	std::string  m_address;
	unsigned short m_port;
	unsigned short m_local;
	unsigned int m_netDGId;
	std::vector<IMRSDestination*> m_destinations;
	unsigned int m_rfHangTime;
	unsigned int m_netHangTime;
	bool         m_debug;
};

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
	bool         getBleep() const;
	bool         getDebug() const;
	bool         getDaemon() const;

	// The Info section
	unsigned int getRxFrequency() const;
	unsigned int getTxFrequency() const;
	unsigned int getPower() const;
	float        getLatitude() const;
	float        getLongitude() const;
	int          getHeight() const;
	std::string  getDescription() const;

	// The Log section
	unsigned int getLogDisplayLevel() const;
	unsigned int getLogMQTTLevel() const;

	// The APRS section
	bool         getAPRSEnabled() const;
	std::string  getAPRSSuffix() const;
	std::string  getAPRSDescription() const;
	std::string  getAPRSSymbol() const;

	// The MQTT section
	std::string  getMQTTAddress() const;
	unsigned short getMQTTPort() const;
	unsigned int getMQTTKeepalive() const;
	std::string  getMQTTName() const;
	bool         getMQTTAuthEnabled() const;
	std::string  getMQTTUsername() const;
	std::string  getMQTTPassword() const;

	// The YSF Network section
	std::string  getYSFNetHosts() const;

	// The DG-ID Section
	std::vector<DGIdData*> getDGIdData() const;

	// The GPSD section
	bool         getGPSDEnabled() const;
	std::string  getGPSDAddress() const;
	std::string  getGPSDPort() const;

private:
	std::string  m_file;
	std::string  m_callsign;
	std::string  m_suffix;
	unsigned int m_id;
	std::string  m_rptAddress;
	unsigned short m_rptPort;
	std::string  m_myAddress;
	unsigned short m_myPort;
	unsigned int m_rfHangTime;
	unsigned int m_netHangTime;
	bool         m_bleep;
	bool         m_debug;
	bool         m_daemon;

	unsigned int m_rxFrequency;
	unsigned int m_txFrequency;
	unsigned int m_power;
	float        m_latitude;
	float        m_longitude;
	int          m_height;
	std::string  m_description;

	unsigned int m_logDisplayLevel;
	unsigned int m_logMQTTLevel;

	bool         m_aprsEnabled;
	std::string  m_aprsSuffix;
	std::string  m_aprsDescription;
	std::string  m_aprsSymbol;

	std::string  m_mqttAddress;
	unsigned short m_mqttPort;
	unsigned int m_mqttKeepalive;
	std::string  m_mqttName;
	bool         m_mqttAuthEnabled;
	std::string  m_mqttUsername;
	std::string  m_mqttPassword;

	std::string  m_ysfNetHosts;
	unsigned int m_ysfRFHangTime;
	unsigned int m_ysfNetHangTime;
	bool         m_ysfNetDebug;

	unsigned int m_fcsRFHangTime;
	unsigned int m_fcsNetHangTime;
	bool         m_fcsNetDebug;

	unsigned int m_imrsRFHangTime;
	unsigned int m_imrsNetHangTime;
	bool         m_imrsNetDebug;

	std::vector<DGIdData*> m_dgIdData;

	bool         m_gpsdEnabled;
	std::string  m_gpsdAddress;
	std::string  m_gpsdPort;
};

#endif
