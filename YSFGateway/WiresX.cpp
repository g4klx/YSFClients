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

const unsigned char DX_REQ[]    = {0x5DU, 0x71U, 0x5FU};
const unsigned char CONN_REQ[]  = {0x5DU, 0x23U, 0x5FU};
const unsigned char DISC_REQ[]  = {0x5DU, 0x2AU, 0x5FU};
const unsigned char ALL_REQ[]   = {0x5DU, 0x66U, 0x5FU};

const unsigned char DX_RESP[]   = {0x5DU, 0x51U, 0x5FU, 0x26U};
const unsigned char CONN_RESP[] = {0x5DU, 0x41U, 0x5FU, 0x26U};
const unsigned char DISC_RESP[] = {0x5DU, 0x41U, 0x5FU, 0x26U};
const unsigned char ALL_RESP[]  = {0x5DU, 0x46U, 0x5FU, 0x26U};

const unsigned char DEFAULT_FICH[] = {0x20U, 0x00U, 0x01U, 0x00U};

const unsigned char NET_HEADER[] = "YSFDGATEWAY             ALL       ";

CWiresX::CWiresX(CNetwork* network, const std::string& hostsFile, unsigned int statusPort) :
m_network(network),
m_reflectors(hostsFile, statusPort),
m_reflector(NULL),
m_id(),
m_name(),
m_description(),
m_txFrequency(0U),
m_rxFrequency(0U),
m_timer(1000U, 0U, 100U + 750U),
m_seqNo(0U),
m_csd1(NULL),
m_status(WXSI_NONE)
{
	assert(network != NULL);
	assert(statusPort > 0U);

	m_csd1 = new unsigned char[20U];
}

CWiresX::~CWiresX()
{
	delete[] m_csd1;
}

void CWiresX::setInfo(const std::string& name, const std::string& description, unsigned int txFrequency, unsigned int rxFrequency)
{
	assert(txFrequency > 0U);
	assert(rxFrequency > 0U);

	m_name        = name;
	m_description = description;
	m_txFrequency = txFrequency;
	m_rxFrequency = rxFrequency;

	unsigned int hash = 0U;

	for (unsigned int i = 0U; i < name.size(); i++) {
		hash += name.at(i);
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	// Final avalanche
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	char id[10U];
	::sprintf(id, "%05u", hash % 100000U);

	LogInfo("The ID of this repeater is %s", id);

	m_id = std::string(id);
}

bool CWiresX::start()
{
	return m_reflectors.load();
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

		if (::memcmp(buffer + 1U, DX_REQ, 3U) == 0) {
			processDX();
			return WXS_NONE;
		} else if (::memcmp(buffer + 1U, ALL_REQ, 3U) == 0) {
			processAll();
			return WXS_NONE;
		} else if (::memcmp(buffer + 1U, CONN_REQ, 3U) == 0) {
			return processConnect(buffer + 5U);
		} else if (::memcmp(buffer + 1U, DISC_REQ, 3U) == 0) {
			processDisconnect();
			return WXS_DISCONNECT;
		} else {
			::memset(m_csd1, ' ', 20U);
			return WXS_NONE;
		}
	}

	return WXS_NONE;
}

CYSFReflector* CWiresX::getReflector() const
{
	return m_reflector;
}

void CWiresX::processDX()
{
	::LogDebug("Received DX from %10.10s", m_csd1 + 10U);

	m_status = WXSI_DX;
	m_timer.start();
}

void CWiresX::processAll()
{
	m_status = WXSI_ALL;
	m_timer.start();
}

WX_STATUS CWiresX::processConnect(const unsigned char* data)
{
	::LogDebug("Received Connect to %5.5s from %10.10s", data + 5U, m_csd1 + 10U);

	std::string id = std::string((char*)(data + 4U), 5U);

	m_reflector = m_reflectors.find(id);
	if (m_reflector == NULL)
		return WXS_NONE;

	m_status = WXSI_CONNECT;
	m_timer.start();

	return WXS_CONNECT;
}

void CWiresX::processDisconnect()
{
	::LogDebug("Received Disconect from %10.10s", m_csd1 + 10U);

	m_status = WXSI_DISCONNECT;
	m_timer.start();
}

void CWiresX::clock(unsigned int ms)
{
	m_timer.clock(ms);
	if (m_timer.isRunning() && m_timer.hasExpired()) {
		LogDebug("Send reply");

		switch (m_status) {
		case WXSI_DX:
			sendDXReply();
			break;
		case WXSI_ALL:
			sendAllReply();
			break;
		case WXSI_CONNECT:
			sendConnectReply();
			break;
		case WXSI_DISCONNECT:
			sendDisconnectReply();
			break;
		default:
			break;
		}

		m_status = WXSI_NONE;
		m_timer.stop();
	}
}

void CWiresX::createReply(const unsigned char* data, unsigned int length)
{
	assert(data != NULL);
	assert(length > 0U);

	unsigned char ft = calculateFT(length);
	unsigned char bt = length / 260U;

	// Write the header
	unsigned char buffer[200U];
	::memcpy(buffer, NET_HEADER, 34U);
	buffer[34U] = 0x00U;

	CSync::add(buffer + 35U);

	CYSFFICH fich;
	fich.load(DEFAULT_FICH);
	fich.setFI(YSF_FI_HEADER);
	fich.setBT(bt);
	fich.setFT(ft);
	fich.encode(buffer + 35U);

	CYSFPayload payload;
	payload.writeDataFRModeData1(m_csd1, buffer + 35U);
	// payload.writeDataFRModeData2("                   ", buffer + 35U);

	m_network->write(buffer);

	fich.setFI(YSF_FI_COMMUNICATIONS);

	unsigned char fn = 0U;
	unsigned char bn = 0U;

	unsigned int offset = 0U;
	while (offset < length) {
		switch (fn) {
		case 0U: {
				unsigned int len = length - offset;
				ft = calculateFT(len);
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

		fich.setFT(ft);
		fich.setFN(fn);
		fich.setBT(bt);
		fich.setBN(bn);
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

	fich.setFI(YSF_FI_TERMINATOR);
	fich.setFN(fn);
	fich.setBN(bn);
	fich.encode(buffer + 35U);

	payload.writeDataFRModeData1(m_csd1, buffer + 35U);
	// payload.writeDataFRModeData2("                   ", buffer + 35U);

	m_network->write(buffer);
}

unsigned char CWiresX::calculateFT(unsigned int length) const
{
	if (length > 220U) return 7U;

	if (length > 180U) return 6U;

	if (length > 140U) return 5U;

	if (length > 100U) return 4U;

	if (length > 60U)  return 3U;

	if (length > 20U)  return 2U;

	return 1U;
}

void CWiresX::sendDXReply()
{
	unsigned char data[150U];
	::memset(data, 0x00U, 150U);
	::memset(data, ' ', 128U);

	data[0U] = m_seqNo;
	::memcmp(data + 1U, DX_RESP, 4U);

	data[127U] = 0x03U;			// End of data marker
	data[128U] = CCRC::addCRC(data, 128U);

	createReply(data, 140U);

	m_seqNo++;
}

void CWiresX::sendConnectReply()
{

}

void CWiresX::sendDisconnectReply()
{

}

void CWiresX::sendAllReply()
{

}
