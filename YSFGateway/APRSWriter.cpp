/*
 *   Copyright (C) 2010-2014,2016,2017,2018,2020 by Jonathan Naylor G4KLX
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

#include "APRSWriter.h"

#include "YSFDefines.h"

#include <cstdio>
#include <cassert>
#include <cstring>
#include <cmath>

CAPRSWriter::CAPRSWriter(const std::string& callsign, const std::string& rptSuffix, const std::string& password, const std::string& address, unsigned int port, const std::string& suffix) :
m_thread(NULL),
m_idTimer(1000U),
m_callsign(callsign),
m_txFrequency(0U),
m_rxFrequency(0U),
m_latitude(0.0F),
m_longitude(0.0F),
m_height(0),
m_desc(),
m_suffix(suffix),
m_mobileGPSAddress(),
m_mobileGPSPort(0U),
m_socket(NULL)
{
	assert(!callsign.empty());
	assert(!password.empty());
	assert(!address.empty());
	assert(port > 0U);

	if (!rptSuffix.empty()) {
		m_callsign.append("-");
		m_callsign.append(rptSuffix.substr(0U, 1U));
	}

	m_thread = new CAPRSWriterThread(m_callsign, password, address, port);
}

CAPRSWriter::~CAPRSWriter()
{
}

void CAPRSWriter::setInfo(unsigned int txFrequency, unsigned int rxFrequency, const std::string& desc)
{
	m_txFrequency = txFrequency;
	m_rxFrequency = rxFrequency;
	m_desc        = desc;
}

void CAPRSWriter::setStaticLocation(float latitude, float longitude, int height)
{
	m_latitude  = latitude;
	m_longitude = longitude;
	m_height    = height;
}

void CAPRSWriter::setMobileLocation(const std::string& address, unsigned int port)
{
	assert(!address.empty());
	assert(port > 0U);

	m_mobileGPSAddress = CUDPSocket::lookup(address);
	m_mobileGPSPort    = port;

	m_socket = new CUDPSocket;
}

bool CAPRSWriter::open()
{
	if (m_socket != NULL) {
		bool ret = m_socket->open();
		if (!ret) {
			delete m_socket;
			m_socket = NULL;
			return false;
		}

		// Poll the GPS every minute
		m_idTimer.setTimeout(60U);
	} else {
		m_idTimer.setTimeout(20U * 60U);
	}

	m_idTimer.start();

	return m_thread->start();
}

void CAPRSWriter::write(const unsigned char* source, const char* type, unsigned char radio, float fLatitude, float fLongitude)
{
	assert(source != NULL);
	assert(type != NULL);

	char callsign[15U];
	::memcpy(callsign, source, YSF_CALLSIGN_LENGTH);
	callsign[YSF_CALLSIGN_LENGTH] = 0x00U;

	size_t n = ::strspn(callsign, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	callsign[n] = 0x00U;

	if (!m_suffix.empty()) {
		::strcat(callsign, "-");
		::strcat(callsign, m_suffix.substr(0U, 1U).c_str());
	}

	double tempLat = ::fabs(fLatitude);
	double tempLong = ::fabs(fLongitude);

	double latitude = ::floor(tempLat);
	double longitude = ::floor(tempLong);

	latitude = (tempLat - latitude)  * 60.0 + latitude  * 100.0;
	longitude = (tempLong - longitude) * 60.0 + longitude * 100.0;

	char lat[20U];
	::sprintf(lat, "%07.2lf", latitude);

	char lon[20U];
	::sprintf(lon, "%08.2lf", longitude);

	char symbol;
	switch (radio) {
	case 0x24U:
	case 0x28U:
	case 0x30U:
		symbol = '[';
		break;
	case 0x25U:
	case 0x29U:
		symbol = '>';
		break;
	case 0x26U:
		symbol = 'r';
		break;
	default:
		symbol = '-';
		break;
	}

	char output[300U];
	::sprintf(output, "%s>APDPRS,C4FM*,qAR,%s:!%s%c/%s%c%c %s via MMDVM",
		callsign, m_callsign.c_str(),
		lat, (fLatitude < 0.0F) ? 'S' : 'N',
		lon, (fLongitude < 0.0F) ? 'W' : 'E',
		symbol, type);

	m_thread->write(output);
}

void CAPRSWriter::clock(unsigned int ms)
{
	m_idTimer.clock(ms);

	m_thread->clock(ms);

	if (m_socket != NULL) {
		if (m_idTimer.hasExpired()) {
			pollGPS();
			m_idTimer.start();
		}

		sendIdFrameMobile();
	} else {
		if (m_idTimer.hasExpired()) {
			sendIdFrameFixed();
			m_idTimer.start();
		}
	}
}

void CAPRSWriter::close()
{
	if (m_socket != NULL) {
		m_socket->close();
		delete m_socket;
	}

	m_thread->stop();
}

bool CAPRSWriter::pollGPS()
{
	assert(m_socket != NULL);

	return m_socket->write((unsigned char*)"YSFGateway", 10U, m_mobileGPSAddress, m_mobileGPSPort);
}

void CAPRSWriter::sendIdFrameFixed()
{
	if (!m_thread->isConnected())
		return;

	// Default values aren't passed on
	if (m_latitude == 0.0F && m_longitude == 0.0F)
		return;

	char desc[200U];
	if (m_txFrequency != 0U) {
		float offset = float(int(m_rxFrequency) - int(m_txFrequency)) / 1000000.0F;
		::sprintf(desc, "MMDVM Voice %.5LfMHz %c%.4lfMHz%s%s",
			(long double)(m_txFrequency) / 1000000.0F,
			offset < 0.0F ? '-' : '+',
			::fabs(offset), m_desc.empty() ? "" : ", ", m_desc.c_str());
	} else {
		::sprintf(desc, "MMDVM Voice%s%s", m_desc.empty() ? "" : ", ", m_desc.c_str());
	}

	const char* band = "4m";
	if (m_txFrequency >= 1200000000U)
		band = "1.2";
	else if (m_txFrequency >= 420000000U)
		band = "440";
	else if (m_txFrequency >= 144000000U)
		band = "2m";
	else if (m_txFrequency >= 50000000U)
		band = "6m";
	else if (m_txFrequency >= 28000000U)
		band = "10m";

	double tempLat  = ::fabs(m_latitude);
	double tempLong = ::fabs(m_longitude);

	double latitude  = ::floor(tempLat);
	double longitude = ::floor(tempLong);

	latitude  = (tempLat  - latitude)  * 60.0 + latitude  * 100.0;
	longitude = (tempLong - longitude) * 60.0 + longitude * 100.0;

	char lat[20U];
	::sprintf(lat, "%07.2lf", latitude);

	char lon[20U];
	::sprintf(lon, "%08.2lf", longitude);

	std::string server = m_callsign;
	size_t pos = server.find_first_of('-');
	if (pos == std::string::npos)
		server.append("-S");
	else
		server.append("S");

	char output[500U];
	::sprintf(output, "%s>APDG03,TCPIP*,qAC,%s:!%s%cD%s%c&/A=%06.0f%s %s",
		m_callsign.c_str(), server.c_str(),
		lat, (m_latitude < 0.0F)  ? 'S' : 'N',
		lon, (m_longitude < 0.0F) ? 'W' : 'E',
		float(m_height) * 3.28F, band, desc);

	m_thread->write(output);
}

void CAPRSWriter::sendIdFrameMobile()
{
	// Grab GPS data if it's available
	unsigned char buffer[200U];
	in_addr address;
	unsigned int port;
	int ret = m_socket->read(buffer, 200U, address, port);
	if (ret <= 0)
		return;

	if (!m_thread->isConnected())
		return;

	buffer[ret] = '\0';

	// Parse the GPS data
	char* pLatitude  = ::strtok((char*)buffer, ",\n");	// Latitude
	char* pLongitude = ::strtok(NULL, ",\n");		// Longitude
	char* pAltitude  = ::strtok(NULL, ",\n");		// Altitude (m)
	char* pVelocity  = ::strtok(NULL, ",\n");		// Velocity (kms/h)
	char* pBearing   = ::strtok(NULL, "\n");		// Bearing

	if (pLatitude == NULL || pLongitude == NULL || pAltitude == NULL)
		return;

	float rawLatitude  = float(::atof(pLatitude));
	float rawLongitude = float(::atof(pLongitude));
	float rawAltitude  = float(::atof(pAltitude));

	char desc[200U];
	if (m_txFrequency != 0U) {
		float offset = float(int(m_rxFrequency) - int(m_txFrequency)) / 1000000.0F;
		::sprintf(desc, "MMDVM Voice %.5LfMHz %c%.4lfMHz%s%s",
			(long double)(m_txFrequency) / 1000000.0F,
			offset < 0.0F ? '-' : '+',
			::fabs(offset), m_desc.empty() ? "" : ", ", m_desc.c_str());
	} else {
		::sprintf(desc, "MMDVM Voice%s%s", m_desc.empty() ? "" : ", ", m_desc.c_str());
	}

	const char* band = "4m";
	if (m_txFrequency >= 1200000000U)
		band = "1.2";
	else if (m_txFrequency >= 420000000U)
		band = "440";
	else if (m_txFrequency >= 144000000U)
		band = "2m";
	else if (m_txFrequency >= 50000000U)
		band = "6m";
	else if (m_txFrequency >= 28000000U)
		band = "10m";

	double tempLat  = ::fabs(rawLatitude);
	double tempLong = ::fabs(rawLongitude);

	double latitude  = ::floor(tempLat);
	double longitude = ::floor(tempLong);

	latitude  = (tempLat  - latitude)  * 60.0 + latitude  * 100.0;
	longitude = (tempLong - longitude) * 60.0 + longitude * 100.0;

	char lat[20U];
	::sprintf(lat, "%07.2lf", latitude);

	char lon[20U];
	::sprintf(lon, "%08.2lf", longitude);

	std::string server = m_callsign;
	size_t pos = server.find_first_of('-');
	if (pos == std::string::npos)
		server.append("-S");
	else
		server.append("S");

	char output[500U];
	::sprintf(output, "%s>APDG03,TCPIP*,qAC,%s:!%s%cD%s%c&",
		m_callsign.c_str(), server.c_str(),
		lat, (rawLatitude < 0.0F)  ? 'S' : 'N',
		lon, (rawLongitude < 0.0F) ? 'W' : 'E');

	if (pBearing != NULL && pVelocity != NULL) {
		float rawBearing   = float(::atof(pBearing));
		float rawVelocity  = float(::atof(pVelocity));

		::sprintf(output + ::strlen(output), "%03.0f/%03.0f", rawBearing, rawVelocity * 0.539957F);
	}

	::sprintf(output + ::strlen(output), "/A=%06.0f%s %s", float(rawAltitude) * 3.28F, band, desc);

	m_thread->write(output);
}
