/*
*   Copyright (C) 2016,2017,2018 by Jonathan Naylor G4KLX
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

#include "GPS.h"
#include "YSFPayload.h"
#include "YSFDefines.h"
#include "Utils.h"
#include "CRC.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>

const unsigned char SHRT_GPS[] = {0x22U, 0x62U};
const unsigned char LONG_GPS[] = {0x47U, 0x64U};

CGPS::CGPS(CAPRSWriter* writer) :
m_writer(writer),
m_buffer(NULL),
m_sent(false)
{
	assert(writer != NULL);

	m_buffer = new unsigned char[300U];
}

CGPS::~CGPS()
{
	delete[] m_buffer;
}

void CGPS::data(const unsigned char* source, const unsigned char* data, unsigned char fi, unsigned char dt, unsigned char fn, unsigned char ft)
{
	if (m_sent)
		return;

	if (fi != YSF_FI_COMMUNICATIONS)
		return;

	CYSFPayload payload;

	if (dt == YSF_DT_VD_MODE1) {
		if (fn == 0U || fn == 1U || fn == 2U)
			return;

		bool valid = payload.readVDMode1Data(data, m_buffer + (fn - 3U) * 20U);
		if (!valid)
			return;

		if (fn == ft) {
			bool valid = false;

			// Find the end marker
			for (unsigned int i = (fn - 2U) * 20U; i > 0U; i--) {
				if (m_buffer[i] == 0x03U) {
					unsigned char crc = CCRC::addCRC(m_buffer, i + 1U);
					if (crc == m_buffer[i + 1U])
						valid = true;

					break;
				}
			}

			if (valid) {
				if (::memcmp(m_buffer + 1U, SHRT_GPS, 2U) == 0) {
					CUtils::dump("Short GPS data received", m_buffer, (fn - 2U) * 20U);
					transmitGPS(source);
				}

				if (::memcmp(m_buffer + 1U, LONG_GPS, 2U) == 0) {
					CUtils::dump("Long GPS data received", m_buffer, (fn - 2U) * 20U);
					transmitGPS(source);
				}

				m_sent = true;
			}
		}
	} else if (dt == YSF_DT_VD_MODE2) {
		if (fn != 6U && fn != 7U)
			return;

		bool valid = payload.readVDMode2Data(data, m_buffer + (fn - 6U) * 10U);
		if (!valid)
			return;

		if (fn == ft) {
			bool valid = false;

			// Find the end marker
			for (unsigned int i = (fn - 5U) * 10U; i > 0U; i--) {
				if (m_buffer[i] == 0x03U) {
					unsigned char crc = CCRC::addCRC(m_buffer, i + 1U);
					if (crc == m_buffer[i + 1U])
						valid = true;
					break;
				}
			}

			if (valid) {
				if (::memcmp(m_buffer + 1U, SHRT_GPS, 2U) == 0) {
					CUtils::dump("Short GPS data received", m_buffer, (fn - 5U) * 10U);
					transmitGPS(source);
				}

				if (::memcmp(m_buffer + 1U, LONG_GPS, 2U) == 0) {
					CUtils::dump("Long GPS data received", m_buffer, (fn - 5U) * 10U);
					transmitGPS(source);
				}

				m_sent = true;
			}
		}
	}
}

void CGPS::reset()
{
	m_sent = false;
}

void CGPS::transmitGPS(const unsigned char* source)
{
	assert(m_writer != NULL);

	// We don't know who its from!
	if (::memcmp(source, "          ", YSF_CALLSIGN_LENGTH) == 0)
		return;

	for (unsigned int i = 5U; i < 11U; i++) {
		unsigned char b = m_buffer[i] & 0xF0U;
		if (b != 0x50U && b != 0x30U)
			return;                                // error/unknown
	}

	unsigned int tens = m_buffer[5U] & 0x0FU;
	unsigned int units = m_buffer[6U] & 0x0FU;
	unsigned int lat_deg = (tens * 10U) + units;
	if (tens > 9U || units > 9U || lat_deg > 89U)
		return;                                // error/unknown

	tens = m_buffer[7U] & 0x0FU;
	units = m_buffer[8U] & 0x0FU;
	unsigned int lat_min = (tens * 10U) + units;
	if (tens > 9U || units > 9U || lat_min > 59U)
		return;                                // error/unknown

	tens = m_buffer[9U] & 0x0FU;
	units = m_buffer[10U] & 0x0FU;
	unsigned int lat_min_frac = (tens * 10U) + units;
	if (tens > 9U || units > 10U || lat_min_frac > 99U)    // units > 10 ??? .. more buggy Yaesu firmware ?
		return;                                // error/unknown

	int lat_dir;
	unsigned char b = m_buffer[8U] & 0xF0U;                            // currently a guess
	if (b == 0x50U)
		lat_dir = 1;                           // N
	else if (b == 0x30U)
		lat_dir = -1;                          // S
	else
		return;                                // error/unknown

	unsigned int lon_deg;
	b = m_buffer[9U] & 0xF0U;
	if (b == 0x50U) {
	    // lon deg 0 to 9, and 100 to 179
		b = m_buffer[11U];
		if (b >= 0x76U && b <= 0x7FU)
			lon_deg = b - 0x76U;               // 0 to 9
		else if (b >= 0x6CU && b <= 0x75U)
			lon_deg = 100U + (b - 0x6CU);      // 100 to 109
		else if (b >= 0x26U && b <= 0x6BU)
			lon_deg = 110U + (b - 0x26U);      // 110 to 179
		else
			return;                            // error/unknown
	} else if (b == 0x30U) {
	    // lon deg 10 to 99
		b = m_buffer[11U];
		if (b >= 0x26U && b <= 0x7FU)
			lon_deg = 10U + (b - 0x26U);       // 10 to 99
		else
			return;                            // error/unknown
	} else {
		return;                                // error/unknown
	}

	unsigned int lon_min;
	b = m_buffer[12U];
	if (b >= 0x58U && b <= 0x61U)
		lon_min = b - 0x58U;                   // 0 to 9
	else if (b >= 0x26U && b <= 0x57U)
		lon_min = 10U + (b - 0x26U);            // 10 to 59
	else
		return;                                // error/unknown

	unsigned int lon_min_frac;
	b = m_buffer[13U];
	if (b >= 0x1CU && b <= 0x7FU)
		lon_min_frac = b - 0x1CU;
	else
		return;                                // error/unknown

	int lon_dir;
	b = m_buffer[10U] & 0xF0U;
	if (b == 0x30U)
		lon_dir = 1;                           // E
	else if (b == 0x50U)
		lon_dir = -1;                          // W
	else
		return;                                // error/unknown

	unsigned int lat_sec = lat_min_frac * 60U;
	lat_sec = (lat_sec + (lat_sec % 100U)) / 100U;    // with rounding

	unsigned int lon_sec = lon_min_frac * 60U;
	lon_sec = (lon_sec + (lon_sec % 100U)) / 100U;    // with rounding

	// >= 0 is north, < 0 is south
	float latitude = lat_deg + ((lat_min + ((float)lat_min_frac * 0.01F)) * (1.0F / 60.0F));
	latitude *= lat_dir;

	// >= 0 is east, < 0 is west
	float longitude = lon_deg + ((lon_min + ((float)lon_min_frac * 0.01F)) * (1.0F / 60.0F));
	longitude *= lon_dir;

	char radio[10U];

	switch (m_buffer[4U]) {
	case 0x20U:
		::strcpy(radio, "DR-2X");
		break;
	case 0x24U:
		::strcpy(radio, "FT-1D");
		break;
	case 0x25U:
		::strcpy(radio, "FTM-400D");
		break;
	case 0x26U:
		::strcpy(radio, "DR-1X");
		break;
	case 0x28U:
		::strcpy(radio, "FT-2D");
		break;
	case 0x29U:
		::strcpy(radio, "FTM-100D");
		break;
	case 0x31U:  
		::strcpy(radio, "FTM-300D");
		break;					
	case 0x30U:
		::strcpy(radio, "FT-3D");
		break;
	default:
		::sprintf(radio, "0x%02X", m_buffer[4U]);
		break;
	}

	LogMessage("GPS Position from %10.10s of radio=%s lat=%f long=%f", source, radio, latitude, longitude);

	m_writer->write(source, radio, m_buffer[4U], latitude, longitude);

	m_sent = true;
}
