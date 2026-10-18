/*
 *   Copyright (C) 2010-2014,2016,2017,2018,2020,2025 by Jonathan Naylor G4KLX
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
#include "MQTTConnection.h"
#include "YSFDefines.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>
#include <cmath>

// In Log.cpp
extern CMQTTConnection* m_mqtt;

#if defined(USE_GPSD)
// gpsd beaconing: first fix, then 60 s while moving, hourly when parked.
// 50 m clears GPS wander and the ~18 m APRS position resolution.
const unsigned int MOBILE_MIN_INTERVAL_SECS = 60U;
const unsigned int STATIONARY_INTERVAL_SECS = 3600U;
const double       MOVED_DISTANCE_METRES    = 50.0;
#endif

CAPRSWriter::CAPRSWriter(const std::string& callsign, const std::string& rptSuffix, const std::string& suffix, bool debug) :
m_idTimer(1000U),
m_callsign(callsign),
m_debug(debug),
m_txFrequency(0U),
m_rxFrequency(0U),
m_latitude(0.0F),
m_longitude(0.0F),
m_height(0),
m_desc(),
m_symbol(),
m_suffix(suffix)
#if defined(USE_GPSD)
,m_gpsdEnabled(false),
m_gpsdAddress(),
m_gpsdPort(),
m_gpsdData(),
m_gpsdConnected(false),
m_gpsdWarned(false),
m_gpsdTimer(1000U, 30U),
m_fixTimer(1000U, 30U),
m_fixValid(false),
m_fixLatitude(0.0),
m_fixLongitude(0.0),
m_fixAltitudeValid(false),
m_fixAltitude(0.0),
m_fixMotionValid(false),
m_fixSpeed(0.0),
m_fixTrack(0.0),
m_refreshTimer(1000U, STATIONARY_INTERVAL_SECS),
m_beaconSent(false),
m_sentLatitude(0.0),
m_sentLongitude(0.0)
#endif
{
	assert(!callsign.empty());

	if (!rptSuffix.empty()) {
		m_callsign.append("-");
		m_callsign.append(rptSuffix.substr(0U, 1U));
	}
}

CAPRSWriter::~CAPRSWriter()
{
}

void CAPRSWriter::setInfo(unsigned int txFrequency, unsigned int rxFrequency, const std::string& desc, const std::string& symbol)
{
	m_txFrequency = txFrequency;
	m_rxFrequency = rxFrequency;
	m_desc        = desc;
	m_symbol      = symbol;
}

void CAPRSWriter::setStaticLocation(float latitude, float longitude, int height)
{
	m_latitude  = latitude;
	m_longitude = longitude;
	m_height    = height;
}

void CAPRSWriter::setGPSDLocation(const std::string& address, const std::string& port)
{
#if defined(USE_GPSD)
	assert(!address.empty());
	assert(!port.empty());

	m_gpsdEnabled = true;
	m_gpsdAddress = address;
	m_gpsdPort    = port;
#endif
}

bool CAPRSWriter::open()
{
#if defined(USE_GPSD)
	// gpsd may not be up yet; clock() retries
	if (m_gpsdEnabled && !connectGPSD())
		m_gpsdTimer.start();
#endif
	m_idTimer.setTimeout(60U);
	m_idTimer.start();

	return true;
}

void CAPRSWriter::write(const unsigned char* source, const char* type, unsigned char radio, float fLatitude, float fLongitude)
{
	assert(source != nullptr);
	assert(type != nullptr);

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
	case 0x33U:
		symbol = '[';
		break;
	case 0x25U:
	case 0x29U:
	case 0x31U:
		symbol = '>';
		break;
	case 0x20U:
	case 0x26U:
		symbol = 'r';
		break;
	default:
		symbol = '-';
		break;
	}

	char output[300U];
	::sprintf(output, "%s>APDPRS,C4FM*,qAR,%s:!%s%c/%s%c%c %s via MMDVM\r\n",
		callsign, m_callsign.c_str(),
		lat, (fLatitude < 0.0F) ? 'S' : 'N',
		lon, (fLongitude < 0.0F) ? 'W' : 'E',
		symbol, type);

	if (m_debug)
		LogDebug("APRS ==> %s", output);

	m_mqtt->publish("aprs-gateway/aprs", output);
}

void CAPRSWriter::clock(unsigned int ms)
{
	m_idTimer.clock(ms);

#if defined(USE_GPSD)
	if (m_gpsdEnabled) {
		if (m_gpsdConnected) {
			readGPSD();
		} else {
			m_gpsdTimer.clock(ms);
			if (m_gpsdTimer.hasExpired()) {
				if (connectGPSD())
					m_gpsdTimer.stop();
				else
					m_gpsdTimer.start();
			}
		}

		// Don't beacon a stale fix
		m_fixTimer.clock(ms);
		if (m_fixTimer.hasExpired()) {
			m_fixValid = false;
			m_fixTimer.stop();
		}

		m_refreshTimer.clock(ms);

		if (m_fixValid && mobileBeaconDue()) {
			sendIdFrameMobile();

			m_beaconSent    = true;
			m_sentLatitude  = m_fixLatitude;
			m_sentLongitude = m_fixLongitude;

			m_idTimer.setTimeout(MOBILE_MIN_INTERVAL_SECS);
			m_idTimer.start();
			m_refreshTimer.start();
		}

	} else {
#endif
		if (m_idTimer.hasExpired()) {
			sendIdFrameFixed();
			m_idTimer.setTimeout(20U * 60U);
			m_idTimer.start();
		}
#if defined(USE_GPSD)
	}
#endif
}

void CAPRSWriter::close()
{
#if defined(USE_GPSD)
	if (m_gpsdEnabled && m_gpsdConnected) {
		::gps_stream(&m_gpsdData, WATCH_DISABLE, nullptr);
		disconnectGPSD();
	}
#endif
}

void CAPRSWriter::sendIdFrameFixed()
{
	// Default values aren't passed on
	if (m_latitude == 0.0F && m_longitude == 0.0F)
		return;

	char desc[200U];
	if (m_txFrequency != 0U) {
		float offset = float(int(m_rxFrequency) - int(m_txFrequency)) / 1000000.0F;
		::sprintf(desc, "MMDVM Voice (C4FM) %.5LfMHz %c%.4lfMHz%s%s",
			(long double)(m_txFrequency) / 1000000.0F,
			offset < 0.0F ? '-' : '+',
			::fabs(offset), m_desc.empty() ? "" : ", ", m_desc.c_str());
	} else {
		::sprintf(desc, "MMDVM Voice (C4FM)%s%s", m_desc.empty() ? "" : ", ", m_desc.c_str());
	}

	const char* band = "4m";
	if (m_txFrequency >= 1200000000U)
		band = "23cm/1.2GHz";
	else if (m_txFrequency >= 420000000U)
		band = "70cm";
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
	std::string symbol = m_symbol;
	size_t pos = server.find_first_of('-');
	if (pos == std::string::npos)
		server.append("-S");
	else
		server.append("S");

        if (symbol.empty())
                symbol.append("D&");

	char output[500U];
	::sprintf(output, "%s>APDG03,TCPIP*,qAC,%s:!%s%c%c%s%c%c/A=%06.0f%s %s\r\n",
		m_callsign.c_str(), server.c_str(),
		lat, (m_latitude < 0.0F)  ? 'S' : 'N', symbol[0],
		lon, (m_longitude < 0.0F) ? 'W' : 'E', symbol[1],
		float(m_height) * 3.28F, band, desc);

	if (m_debug)
		LogDebug("APRS ==> %s", output);

	m_mqtt->publish("aprs-gateway/aprs", output);
}

#if defined(USE_GPSD)
bool CAPRSWriter::connectGPSD()
{
	int ret = ::gps_open(m_gpsdAddress.c_str(), m_gpsdPort.c_str(), &m_gpsdData);
	if (ret != 0) {
		// -1: reason in errno (system error, or a libgps code on old libgps)
		int err = (ret == -1) ? errno : ret;
		const char* reason = (err < 0) ? ::gps_errstr(err) : ::strerror(err);

		// Warn once per outage
		if (!m_gpsdWarned)
			LogWarning("Unable to connect to gpsd at %s:%s - %s, retrying every 30 seconds", m_gpsdAddress.c_str(), m_gpsdPort.c_str(), reason);
		m_gpsdWarned = true;
		return false;
	}

	::gps_stream(&m_gpsdData, WATCH_ENABLE | WATCH_JSON, nullptr);

	m_gpsdConnected = true;
	m_gpsdWarned    = false;

	LogMessage("Connected to GPSD");

	return true;
}

void CAPRSWriter::disconnectGPSD()
{
	::gps_close(&m_gpsdData);

	m_gpsdConnected = false;
	m_fixValid      = false;
	m_fixTimer.stop();
}

// Drain gpsd and keep the latest fix: gps_read() returns one message.
void CAPRSWriter::readGPSD()
{
	// Bounded; the rest is read next time
	for (unsigned int n = 0U; n < 50U && ::gps_waiting(&m_gpsdData, 0); n++) {
		// Non-TPV messages leave stale flags set
		m_gpsdData.set = 0U;

#if GPSD_API_MAJOR_VERSION >= 7
		int ret = ::gps_read(&m_gpsdData, nullptr, 0);
#else
		int ret = ::gps_read(&m_gpsdData);
#endif
		if (ret < 0) {
			LogWarning("Lost the connection to gpsd, retrying every 30 seconds");
			disconnectGPSD();
			m_gpsdWarned = true;
			m_gpsdTimer.start();
			return;
		}

		if (ret == 0)
			return;

		// TPV reports only
		if ((m_gpsdData.set & MODE_SET) != MODE_SET)
			continue;

		// Any 2D/3D fix (GPS, DGPS, RTK...). gpsd doesn't range-check.
		bool latlonSet = (m_gpsdData.set & LATLON_SET) == LATLON_SET;
		double fixLat  = m_gpsdData.fix.latitude;
		double fixLon  = m_gpsdData.fix.longitude;
		bool latlonOK  = std::isfinite(fixLat) && std::isfinite(fixLon) &&
		                 ::fabs(fixLat) <= 90.0 && ::fabs(fixLon) <= 180.0;
		if (m_gpsdData.fix.mode < MODE_2D || !latlonSet || !latlonOK) {
			m_fixValid = false;
			m_fixTimer.stop();
			continue;
		}

		m_fixLatitude  = fixLat;
		m_fixLongitude = fixLon;

#if GPSD_API_MAJOR_VERSION >= 9
		m_fixAltitude = m_gpsdData.fix.altMSL;
#else
		m_fixAltitude = m_gpsdData.fix.altitude;
#endif
		m_fixAltitudeValid = (m_gpsdData.fix.mode >= MODE_3D) && std::isfinite(m_fixAltitude) &&
		                     m_fixAltitude >= -1000.0 && m_fixAltitude <= 100000.0;

		bool speedSet = (m_gpsdData.set & SPEED_SET) == SPEED_SET;
		bool trackSet = (m_gpsdData.set & TRACK_SET) == TRACK_SET;
		m_fixSpeed       = m_gpsdData.fix.speed;
		m_fixTrack       = m_gpsdData.fix.track;
		m_fixMotionValid = speedSet && trackSet && std::isfinite(m_fixSpeed) && std::isfinite(m_fixTrack);

		m_fixValid = true;
		m_fixTimer.start();
	}
}

bool CAPRSWriter::mobileBeaconDue()
{
	// First fix
	if (!m_beaconSent)
		return true;

	// Parked: hourly refresh
	if (m_refreshTimer.hasExpired())
		return true;

	// Moving: at most once a minute
	if (!m_idTimer.hasExpired())
		return false;

	// Distance from the last position sent (equirectangular)
	const double DEG_TO_RAD   = 0.017453292519943295;
	const double EARTH_RADIUS = 6371000.0;

	double dLon = (m_fixLongitude - m_sentLongitude) * DEG_TO_RAD;
	if (dLon > 3.141592653589793)
		dLon -= 2.0 * 3.141592653589793;
	else if (dLon < -3.141592653589793)
		dLon += 2.0 * 3.141592653589793;

	double dLat   = (m_fixLatitude - m_sentLatitude) * DEG_TO_RAD;
	double midLat = (m_fixLatitude + m_sentLatitude) * 0.5 * DEG_TO_RAD;

	double x = dLon * ::cos(midLat);
	double distance = ::sqrt(x * x + dLat * dLat) * EARTH_RADIUS;

	return distance > MOVED_DISTANCE_METRES;
}

void CAPRSWriter::sendIdFrameMobile()
{
	if (!m_fixValid)
		return;

	double rawLatitude  = m_fixLatitude;
	double rawLongitude = m_fixLongitude;

	char desc[200U];
	if (m_txFrequency != 0U) {
		float offset = float(int(m_rxFrequency) - int(m_txFrequency)) / 1000000.0F;
		::sprintf(desc, "MMDVM Voice (C4FM) %.5LfMHz %c%.4lfMHz%s%s",
			(long double)(m_txFrequency) / 1000000.0F,
			offset < 0.0F ? '-' : '+',
			::fabs(offset), m_desc.empty() ? "" : ", ", m_desc.c_str());
	} else {
		::sprintf(desc, "MMDVM Voice (C4FM)%s%s", m_desc.empty() ? "" : ", ", m_desc.c_str());
	}

	const char* band = "4m";
	if (m_txFrequency >= 1200000000U)
		band = "23cm/1.2GHz";
	else if (m_txFrequency >= 420000000U)
		band = "70cm";
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
	::snprintf(lat, sizeof(lat), "%07.2lf", latitude);

	char lon[20U];
	::snprintf(lon, sizeof(lon), "%08.2lf", longitude);

	std::string server = m_callsign;
	std::string symbol = m_symbol;
	size_t pos = server.find_first_of('-');
	if (pos == std::string::npos)
		server.append("-S");
	else
		server.append("S");

        if (symbol.empty())
                symbol.append("D&");

	char output[500U];
	::snprintf(output, sizeof(output), "%s>APDG03,TCPIP*,qAC,%s:!%s%c%c%s%c%c",
		m_callsign.c_str(), server.c_str(),
		lat, (rawLatitude < 0.0)  ? 'S' : 'N', symbol[0],
		lon, (rawLongitude < 0.0) ? 'W' : 'E', symbol[1]);

	if (m_fixMotionValid) {
		// Course 001-360 (000 = unknown); speed m/s -> knots
		int course = int(::lround(m_fixTrack)) % 360;
		if (course <= 0)
			course += 360;
		long speed = ::lround(m_fixSpeed * 1.943844);
		if (speed < 0L)
			speed = 0L;
		else if (speed > 999L)
			speed = 999L;
		::snprintf(output + ::strlen(output), sizeof(output) - ::strlen(output), "%03d/%03ld", course, speed);
	}

	if (m_fixAltitudeValid)
		::snprintf(output + ::strlen(output), sizeof(output) - ::strlen(output), "/A=%06.0f", m_fixAltitude * 3.28084);

	::snprintf(output + ::strlen(output), sizeof(output) - ::strlen(output), "%s %s\r\n", band, desc);

	if (m_debug)
		LogDebug("APRS ==> %s", output);

	m_mqtt->publish("aprs-gateway/aprs", output);
}
#endif
