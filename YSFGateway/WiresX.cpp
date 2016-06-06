/*
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
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

#include "WiresX.h"
#include "YSFPayload.h"
#include "YSFFICH.h"
#include "Sync.h"
#include "CRC.h"
#include "Log.h"

#include <cstdio>
#include <cassert>

const unsigned char CALL_DX[]      = {0x5DU, 0x71U};
const unsigned char CALL_CONNECT[] = {0x5DU, 0x41U};
const unsigned char CALL_ALL[]     = {0x5DU, 0x66U};

const unsigned char DEFAULT_FICH[] = {0x20U, 0x00U, 0x15U, 0x00U};

const unsigned char NET_HEADER[] = "YSFDGATEWAY             ALL       ";

CWiresX::CWiresX(CNetwork* network) :
m_network(network),
m_reflector(),
m_timer(1000U, 0U, 100U + 750U),
m_csd1(NULL)
{
	assert(network != NULL);

	m_csd1 = new unsigned char[20U];
}

CWiresX::~CWiresX()
{
	delete[] m_csd1;
}

WX_STATUS CWiresX::process(const unsigned char* data, unsigned char fi, unsigned char dt, unsigned char fn)
{
	assert(data != NULL);

	if (dt != YSF_DT_DATA_FR_MODE)
		return WXS_NONE;

	CYSFPayload payload;

	if (fi == YSF_FI_HEADER) {
		payload.readDataFRModeData1(data, m_csd1);
		return WXS_NONE;
	}

	if (fi == YSF_FI_COMMUNICATIONS && fn == 0U) {
		if (::memcmp(m_csd1, "                    ", 20U) == 0)
			payload.readDataFRModeData1(data, m_csd1);
		return WXS_NONE;
	}

	if (fi == YSF_FI_TERMINATOR) {
		if (::memcmp(m_csd1, "                    ", 20U) == 0)
			payload.readDataFRModeData1(data, m_csd1);
		return WXS_NONE;
	}

	if (fi == YSF_FI_COMMUNICATIONS && fn == 1U) {
		unsigned char buffer[20U];
		bool valid = payload.readDataFRModeData2(data, buffer);
		if (!valid) {
			::memset(m_csd1, ' ', 20U);
			return WXS_NONE;
		}

		if (::memcmp(buffer + 1U, CALL_DX, 2U) == 0)
			processDX();
		else if (::memcmp(buffer + 1U, CALL_ALL, 2U) == 0)
			processAll();
		else if (::memcmp(buffer + 1U, CALL_CONNECT, 2U) == 0)
			return processConnect();
		else
			::memset(m_csd1, ' ', 20U);
	}

	return WXS_NONE;
}

std::string CWiresX::getReflector() const
{
	return m_reflector;
}

void CWiresX::processDX()
{
	::LogDebug("Received DX from %10.10s", m_csd1 + 10U);

	m_timer.start();
}

void CWiresX::processAll()
{

}

WX_STATUS CWiresX::processConnect()
{
	return WXS_NONE;
}

void CWiresX::clock(unsigned int ms)
{
	m_timer.clock(ms);
	if (m_timer.isRunning() && m_timer.hasExpired()) {
		LogDebug("Send reply");

		unsigned char data[150U];
		::memset(data, 0x00U, 150U);
		::memset(data, ' ', 128U);

		data[0U] = 0x03U;			// XXX Serial no
		data[1U] = 0x5DU;
		data[2U] = 0x51U;
		data[3U] = 0x5FU;			// Start of data marker
		data[4U] = 0x26U;			// Repeater type

		data[127U] = 0x03U;			// End of data marker
		data[128U] = CCRC::addCRC(data, 128U);

		createReply(data, 140U);

		m_timer.stop();
	}
}

void CWiresX::createReply(const unsigned char* data, unsigned int length)
{
	assert(data != NULL);
	assert(length > 0U);

	// Write the header
	unsigned char buffer[200U];
	::memcpy(buffer, NET_HEADER, 34U);
	buffer[34U] = 0x00U;

	CSync::add(buffer + 35U);

	CYSFFICH fich;
	fich.load(DEFAULT_FICH);
	fich.setFI(YSF_FI_HEADER);
	fich.encode(buffer + 35U);

	CYSFPayload payload;
	payload.writeDataFRModeData1(m_csd1, buffer + 35U);
	// payload.writeDataFRModeData2("                   ", buffer + 35U);

	m_network->write(buffer);

	unsigned char bt = length / 260U;
	fich.setBT(bt);

	unsigned char fn = 0U;
	unsigned char bn = 0U;

	unsigned int offset = 0U;
	while (offset < length) {
		switch (fn) {
		case 0U: {
				unsigned int len = length - offset;
				if (len > 220U)      fich.setFT(7U);
				else if (len > 180U) fich.setFT(6U);
				else if (len > 140U) fich.setFT(5U);
				else if (len > 100U) fich.setFT(4U);
				else if (len > 60U)  fich.setFT(3U);
				else if (len > 20U)  fich.setFT(2U);
				else                 fich.setFT(1U);
				payload.writeDataFRModeData1(m_csd1, buffer + 35U);
				// payload.writeDataFRModeData2("                   ", buffer + 35U);
			}
			break;
		case 1U:
			// payload.writeDataFRModeData1("                   ", buffer + 35U);
			payload.writeDataFRModeData2(data + offset, buffer + 35U);
			offset += 20U;
			break;
		default:
			payload.writeDataFRModeData1(data + offset, buffer + 35U);
			offset += 20U;
			payload.writeDataFRModeData2(data + offset, buffer + 35U);
			offset += 20U;
			break;
		}

		fich.setFN(fn);
		fich.setBN(bn);
		fich.setFI(YSF_FI_COMMUNICATIONS);
		fich.encode(buffer + 35U);

		m_network->write(buffer);

		fn++;
		if (fn >= 8U) {
			fn = 0U;
			bn++;
		}
	}

	// Write the trailer
	buffer[34U] = 0x01U;

	fich.load(DEFAULT_FICH);
	fich.setFI(YSF_FI_TERMINATOR);
	fich.encode(buffer + 35U);

	payload.writeDataFRModeData1(m_csd1, buffer + 35U);
	// payload.writeDataFRModeData2("                   ", buffer + 35U);

	m_network->write(buffer);
}
