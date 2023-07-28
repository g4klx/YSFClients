/*
*   Copyright (C) 2016,2017,2018,2019,2020 by Jonathan Naylor G4KLX
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
#include "Utils.h"
#include "Sync.h"
#include "CRC.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstdlib>

const unsigned char DX_REQ[]    = {0x5DU, 0x71U, 0x5FU};
const unsigned char CONN_REQ[]  = {0x5DU, 0x23U, 0x5FU};
const unsigned char DISC_REQ[]  = {0x5DU, 0x2AU, 0x5FU};
const unsigned char ALL_REQ[]   = {0x5DU, 0x66U, 0x5FU};
const unsigned char CAT_REQ[]   = {0x5DU, 0x67U, 0x5FU};

const unsigned char DX_RESP[]   = {0x5DU, 0x51U, 0x5FU, 0x26U};
const unsigned char CONN_RESP[] = {0x5DU, 0x41U, 0x5FU, 0x26U};
const unsigned char DISC_RESP[] = {0x5DU, 0x41U, 0x5FU, 0x26U};
const unsigned char ALL_RESP[]  = {0x5DU, 0x46U, 0x5FU, 0x26U};

const unsigned char DEFAULT_FICH[] = {0x20U, 0x00U, 0x01U, 0x00U};

const unsigned char NET_HEADER[] = "YSFD                    ALL      ";

CWiresX::CWiresX(const std::string& callsign, const std::string& suffix, CYSFNetwork* network, CYSFReflectors& reflectors) :
m_callsign(callsign),
m_node(),
m_network(network),
m_reflectors(reflectors),
m_reflector(NULL),
m_id(),
m_name(),
m_command(NULL),
m_txFrequency(0U),
m_rxFrequency(0U),
m_timer(1000U, 1U),
m_seqNo(0U),
m_header(NULL),
m_csd1(NULL),
m_csd2(NULL),
m_csd3(NULL),
m_status(WXSI_NONE),
m_start(0U),
m_search(),
m_busy(false),
m_busyTimer(3000U, 1U),
m_bufferTX(10000U, "YSF Wires-X TX Buffer")
{
	assert(network != NULL);

	m_node = callsign;
	if (suffix.size() > 0U) {
		m_node.append("-");
		m_node.append(suffix);
	}
	m_node.resize(YSF_CALLSIGN_LENGTH, ' ');

	m_callsign.resize(YSF_CALLSIGN_LENGTH, ' ');

	m_command = new unsigned char[300U];

	m_header = new unsigned char[34U];
	m_csd1   = new unsigned char[20U];
	m_csd2   = new unsigned char[20U];
	m_csd3   = new unsigned char[20U];

	m_txWatch.start();
}

CWiresX::~CWiresX()
{
	delete[] m_csd3;
	delete[] m_csd2;
	delete[] m_csd1;
	delete[] m_header;
	delete[] m_command;
}

void CWiresX::setInfo(const std::string& name, unsigned int txFrequency, unsigned int rxFrequency)
{
	assert(txFrequency > 0U);
	assert(rxFrequency > 0U);

	m_name        = name;
	m_txFrequency = txFrequency;
	m_rxFrequency = rxFrequency;

	m_name.resize(14U, ' ');

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

	::memset(m_csd1, '*', 20U);
	::memset(m_csd2, ' ', 20U);
	::memset(m_csd3, ' ', 20U);

	for (unsigned int i = 0U; i < 10U; i++)
		m_csd1[i + 10U] = m_node.at(i);

	for (unsigned int i = 0U; i < 10U; i++)
		m_csd2[i + 0U] = m_callsign.at(i);

	for (unsigned int i = 0U; i < 5U; i++) {
		m_csd3[i + 0U]  = m_id.at(i);
		m_csd3[i + 15U] = m_id.at(i);
	}

	for (unsigned int i = 0U; i < 34U; i++)
		m_header[i] = NET_HEADER[i];

	for (unsigned int i = 0U; i < 10U; i++)
		m_header[i + 4U] = m_callsign.at(i);

	for (unsigned int i = 0U; i < 10U; i++)
		m_header[i + 14U] = m_node.at(i);
}

void CWiresX::setParrot(const std::string& address, unsigned short port)
{
	m_reflectors.setParrot(address, port);
}

void CWiresX::setYSF2DMR(const std::string& address, unsigned short port)
{
	m_reflectors.setYSF2DMR(address, port);
}

void CWiresX::setYSF2NXDN(const std::string& address, unsigned short port)
{
	m_reflectors.setYSF2NXDN(address, port);
}

void CWiresX::setYSF2P25(const std::string& address, unsigned short port)
{
	m_reflectors.setYSF2P25(address, port);
}

void CWiresX::setYSFDirect(const std::string& address, unsigned short port)
{
	m_reflectors.setYSFDirect(address, port);
}

void CWiresX::addFCSRoom(const std::string& id, const std::string& name)
{
	m_reflectors.addFCSRoom(id, name);
}

bool CWiresX::start()
{
	m_reflectors.reload();

	return true;
}

WX_STATUS CWiresX::process(const unsigned char* data, const unsigned char* source, const CYSFFICH& fich, bool wiresXCommandPassthrough)
{
	assert(data != NULL);
	assert(source != NULL);

	unsigned char dt = fich.getDT();
	if (dt != YSF_DT_DATA_FR_MODE)
		return WXS_NONE;

	unsigned char fi = fich.getFI();
	if (fi != YSF_FI_COMMUNICATIONS)
		return WXS_NONE;

	CYSFPayload payload;

	unsigned char fn = fich.getFN();
	if (fn == 0U)
		return WXS_NONE;

	if (fn == 1U) {
		bool valid = payload.readDataFRModeData2(data, m_command + 0U);
		if (!valid)
			return WXS_NONE;
	} else {
		bool valid = payload.readDataFRModeData1(data, m_command + (fn - 2U) * 40U + 20U);
		if (!valid)
			return WXS_NONE;

		valid = payload.readDataFRModeData2(data, m_command + (fn - 2U) * 40U + 40U);
		if (!valid)
			return WXS_NONE;
	}

	unsigned char ft = fich.getFT();
	if (fn == ft) {
		bool valid = false;

		// Find the end marker
		unsigned int cmd_len = (fn - 1U) * 40U + 20U;
		for (unsigned int i = cmd_len; i > 0U; i--) {
			if (m_command[i] == 0x03U) {
				unsigned char crc = CCRC::addCRC(m_command, i + 1U);
				if (crc == m_command[i + 1U])
					valid = true;
				break;
			}
		}

		if (!valid)
			return WXS_NONE;

		CUtils::dump(1U, "Received Wires-X command", m_command, cmd_len);

		// If we are using WiresX Passthrough (we already know we are on a YSF2xxx room from YSFGateway
		if (wiresXCommandPassthrough) {
			if (::memcmp(m_command + 1U, DX_REQ, 3U) == 0) {
				return WXS_NONE;
			} else if (::memcmp(m_command + 1U, ALL_REQ, 3U) == 0) {
				return WXS_NONE;
			} else if (::memcmp(m_command + 1U, CONN_REQ, 3U) == 0) {
				return WXS_NONE;
			} else if (::memcmp(m_command + 1U, DISC_REQ, 3U) == 0) {
				processDisconnect(source);
				return WXS_DISCONNECT;
			} else if (::memcmp(m_command + 1U, CAT_REQ, 3U) == 0) {
				return WXS_NONE;
			} else {
				CUtils::dump("Unknown Wires-X command", m_command, cmd_len);
				return WXS_NONE;
			}
		}
		// Origional Code Here
		else {
			if (::memcmp(m_command + 1U, DX_REQ, 3U) == 0) {
				processDX(source);
				return WXS_NONE;
			} else if (::memcmp(m_command + 1U, ALL_REQ, 3U) == 0) {
				processAll(source, m_command + 5U);
				return WXS_NONE;
			} else if (::memcmp(m_command + 1U, CONN_REQ, 3U) == 0) {
				return processConnect(source, m_command + 5U);
			} else if (::memcmp(m_command + 1U, DISC_REQ, 3U) == 0) {
				processDisconnect(source);
				return WXS_DISCONNECT;
			} else if (::memcmp(m_command + 1U, CAT_REQ, 3U) == 0) {
				processCategory(source, m_command + 5U);
				return WXS_NONE;
			} else {
				CUtils::dump("Unknown Wires-X command", m_command, cmd_len);
				return WXS_NONE;
			}
		}
	}

	return WXS_NONE;
}

CYSFReflector* CWiresX::getReflector() const
{
	return m_reflector;
}

void CWiresX::setReflector(CYSFReflector* reflector)
{
	m_reflector = reflector;
}

void CWiresX::processDX(const unsigned char* source)
{
	::LogDebug("Received DX from %10.10s", source);

	m_status = WXSI_DX;
	m_timer.start();
}

void CWiresX::processCategory(const unsigned char* source, const unsigned char* data)
{
	::LogDebug("Received CATEGORY request from %10.10s", source);

	char buffer[6U];
	::memcpy(buffer, data + 5U, 2U);
	buffer[3U] = 0x00U;

	unsigned int len = atoi(buffer);

	if (len == 0U)
		return;

	if (len > 20U)
		return;

	m_category.clear();

	for (unsigned int j = 0U; j < len; j++) {
		::memcpy(buffer, data + 7U + j * 5U, 5U);
		buffer[5U] = 0x00U;

		std::string id = std::string(buffer, 5U);

		CYSFReflector* refl = m_reflectors.findById(id);
		if (refl)
			m_category.push_back(refl);
	}

	m_status = WXSI_CATEGORY;
	m_timer.start();
}

void CWiresX::processAll(const unsigned char* source, const unsigned char* data)
{
	char buffer[4U];
	::memcpy(buffer, data + 2U, 3U);
	buffer[3U] = 0x00U;

	if (data[0U] == '0' && data[1] == '1') {
		::LogDebug("Received ALL for \"%3.3s\" from %10.10s", data + 2U, source);

		m_start = ::atoi(buffer);
		if (m_start > 0U)
			m_start--;

		m_status = WXSI_ALL;

		m_timer.start();
	} else if (data[0U] == '1' && data[1U] == '1') {
		::LogDebug("Received SEARCH for \"%16.16s\" from %10.10s", data + 5U, source);

		m_start = ::atoi(buffer);
		if (m_start > 0U)
			m_start--;

		m_search = std::string((char*)(data + 5U), 16U);

		m_status = WXSI_SEARCH;

		m_timer.start();
	}
}

WX_STATUS CWiresX::processConnect(const unsigned char* source, const unsigned char* data)
{
	m_busy = true;
	m_busyTimer.start();

	::LogDebug("Received Connect to %5.5s from %10.10s", data, source);

	std::string id = std::string((char*)data, 5U);

	CYSFReflector* reflector_tmp;
	reflector_tmp = m_reflectors.findById(id);
	if (reflector_tmp != NULL)
		m_reflector = reflector_tmp;

	if (m_reflector == NULL)
		return WXS_NONE;

	m_status = WXSI_CONNECT;
	m_timer.start();

	switch (m_reflector->m_type) {
	case YT_YSF:
		return WXS_CONNECT_YSF;
	case YT_FCS:
		return WXS_CONNECT_FCS;
	default:
		return WXS_NONE;
	}
}

void CWiresX::processConnect(CYSFReflector* reflector)
{
	m_busy = true;
	m_busyTimer.start();

	m_reflector = reflector;

	m_status = WXSI_CONNECT;
	m_timer.start();
}

void CWiresX::processDisconnect(const unsigned char* source)
{
	if (source != NULL)
		::LogDebug("Received Disconect from %10.10s", source);

	m_reflector = NULL;

	m_status = WXSI_DISCONNECT;
	m_timer.start();
}

void CWiresX::clock(unsigned int ms)
{
	unsigned char buffer[200U];

	m_reflectors.clock(ms);

	m_timer.clock(ms);
	if (m_timer.isRunning() && m_timer.hasExpired()) {
		switch (m_status) {
		case WXSI_DX:
			sendDXReply();
			break;
		case WXSI_ALL:
			sendAllReply();
			break;
		case WXSI_SEARCH:
			sendSearchReply();
			break;
		case WXSI_CONNECT:
			sendConnectReply();
			break;
		case WXSI_DISCONNECT:
			sendDisconnectReply();
			break;
		case WXSI_CATEGORY:
			sendCategoryReply();
			break;
		default:
			break;
		}

		m_status = WXSI_NONE;
		m_timer.stop();
	}

	if (m_txWatch.elapsed() > 90U) {
		if (!m_bufferTX.isEmpty() && m_bufferTX.dataSize() >= 155U) {
			unsigned char len = 0U;
			m_bufferTX.getData(&len, 1U);
			if (len == 155U) {
				m_bufferTX.getData(buffer, 155U);
				m_network->write(buffer);
			}
		}
		m_txWatch.start();
	}

	m_busyTimer.clock(ms);
	if (m_busyTimer.isRunning() && m_busyTimer.hasExpired()) {
		m_busy = false;
		m_busyTimer.stop();
	}
}

void CWiresX::createReply(const unsigned char* data, unsigned int length, CYSFNetwork* network)
{
	assert(data != NULL);
	assert(length > 0U);

	bool isYSF2XX = true;

	// If we don't explicitly pass a network, use the default one.
	if (network == NULL) {
		isYSF2XX = false;
		network = m_network;
	}

	unsigned char bt = 0U;

	if (length > 260U) {
		bt = 1U;
		bt += (length - 260U) / 259U;

		length += bt;
	}

	if (length > 20U) {
		unsigned int blocks = (length - 20U) / 40U;
		if ((length % 40U) > 0U) blocks++;
		length = blocks * 40U + 20U;
	} else {
		length = 20U;
	}

	unsigned char ft = calculateFT(length, 0U);

	unsigned char seqNo = 0U;

	// Write the header
	unsigned char buffer[200U];
	::memcpy(buffer, m_header, 34U);

	CSync::add(buffer + 35U);

	CYSFFICH fich;
	fich.setRaw(DEFAULT_FICH);
	fich.setFI(YSF_FI_HEADER);
	fich.setBT(bt);
	fich.setFT(ft);
	fich.encode(buffer + 35U);

	CYSFPayload payload;
	payload.writeDataFRModeData1(m_csd1, buffer + 35U);
	payload.writeDataFRModeData2(m_csd2, buffer + 35U);

	buffer[34U] = seqNo;
	seqNo += 2U;

	writeData(buffer, network, isYSF2XX);

	fich.setFI(YSF_FI_COMMUNICATIONS);

	unsigned char fn = 0U;
	unsigned char bn = 0U;

	unsigned int offset = 0U;
	while (offset < length) {
		switch (fn) {
		case 0U: {
				ft = calculateFT(length, offset);
				payload.writeDataFRModeData1(m_csd1, buffer + 35U);
				payload.writeDataFRModeData2(m_csd2, buffer + 35U);
			}
			break;
		case 1U:
			payload.writeDataFRModeData1(m_csd3, buffer + 35U);
			if (bn == 0U) {
				payload.writeDataFRModeData2(data + offset, buffer + 35U);
				offset += 20U;
			} else {
				// All subsequent entries start with 0x00U
				unsigned char temp[20U];
				::memcpy(temp + 1U, data + offset, 19U);
				temp[0U] = 0x00U;
				payload.writeDataFRModeData2(temp, buffer + 35U);
				offset += 19U;
			}
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

		buffer[34U] = seqNo;
		seqNo += 2U;

		writeData(buffer, network, isYSF2XX);

		fn++;
		if (fn >= 8U) {
			fn = 0U;
			bn++;
		}
	}

	// Write the trailer
	fich.setFI(YSF_FI_TERMINATOR);
	fich.setFN(fn);
	fich.setBN(bn);
	fich.encode(buffer + 35U);

	payload.writeDataFRModeData1(m_csd1, buffer + 35U);
	payload.writeDataFRModeData2(m_csd2, buffer + 35U);

	buffer[34U] = seqNo | 0x01U;

	writeData(buffer, network, isYSF2XX);
}

void CWiresX::writeData(const unsigned char* buffer, CYSFNetwork* network, bool isYSF2XX)
{
	if (isYSF2XX) {
		// Send YSF2XXX Wires-X reply directly to the network
		network->write(buffer);
	} else {
		// Send host Wires-X reply using ring buffer
		unsigned char len = 155U;
		m_bufferTX.addData(&len, 1U);
		m_bufferTX.addData(buffer, len);
	}
}

unsigned char CWiresX::calculateFT(unsigned int length, unsigned int offset) const
{
	length -= offset;

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

	for (unsigned int i = 0U; i < 4U; i++)
		data[i + 1U] = DX_RESP[i];

	for (unsigned int i = 0U; i < 5U; i++)
		data[i + 5U] = m_id.at(i);

	for (unsigned int i = 0U; i < 10U; i++)
		data[i + 10U] = m_node.at(i);

	for (unsigned int i = 0U; i < 14U; i++)
		data[i + 20U] = m_name.at(i);

	if (m_reflector == NULL) {
		data[34U] = '1';
		data[35U] = '2';

		data[57U] = '0';
		data[58U] = '0';
		data[59U] = '0';
	} else {
		data[34U] = '1';
		data[35U] = '5';

		for (unsigned int i = 0U; i < 5U; i++)
			data[i + 36U] = m_reflector->m_id.at(i);

		for (unsigned int i = 0U; i < 16U; i++)
			data[i + 41U] = m_reflector->m_name.at(i);

		for (unsigned int i = 0U; i < 3U; i++)
			data[i + 57U] = m_reflector->m_count.at(i);

		for (unsigned int i = 0U; i < 14U; i++)
			data[i + 70U] = m_reflector->m_desc.at(i);
	}

	unsigned int offset;
	char sign;
	if (m_txFrequency >= m_rxFrequency) {
		offset = m_txFrequency - m_rxFrequency;
		sign = '-';
	} else {
		offset = m_rxFrequency - m_txFrequency;
		sign = '+';
	}

	unsigned int freqHz = m_txFrequency % 1000000U;
	unsigned int freqkHz = (freqHz + 500U) / 1000U;

	char freq[30U];
	::sprintf(freq, "%05u.%03u000%c%03u.%06u", m_txFrequency / 1000000U, freqkHz, sign, offset / 1000000U, offset % 1000000U);

	for (unsigned int i = 0U; i < 23U; i++)
		data[i + 84U] = freq[i];

	data[127U] = 0x03U;			// End of data marker
	data[128U] = CCRC::addCRC(data, 128U);

	CUtils::dump(1U, "DX Reply", data, 129U);

	createReply(data, 129U);

	m_seqNo++;
}

void CWiresX::sendConnect(CYSFNetwork* network)
{
	unsigned char data[20U];
	::memset(data, 0x00U, 20U);
	::memset(data, ' ', 16U);

	data[0U] = m_seqNo;

	for (unsigned int i = 0U; i < 3U; i++)
		data[i + 1U] = DX_REQ[i];

	data[15U] = 0x03U;			// End of data marker
	data[16U] = CCRC::addCRC(data, 16U);

	CUtils::dump(1U, "CONNECT", data, 20U);

	createReply(data, 20U, network);

	m_seqNo++;
}

void CWiresX::sendConnectReply()
{
	assert(m_reflector != NULL);

	unsigned char data[110U];
	::memset(data, 0x00U, 110U);
	::memset(data, ' ', 90U);

	data[0U] = m_seqNo;

	for (unsigned int i = 0U; i < 4U; i++)
		data[i + 1U] = CONN_RESP[i];

	for (unsigned int i = 0U; i < 5U; i++)
		data[i + 5U] = m_id.at(i);

	for (unsigned int i = 0U; i < 10U; i++)
		data[i + 10U] = m_node.at(i);

	for (unsigned int i = 0U; i < 14U; i++)
		data[i + 20U] = m_name.at(i);

	data[34U] = '1';
	data[35U] = '5';

	for (unsigned int i = 0U; i < 5U; i++)
		data[i + 36U] = m_reflector->m_id.at(i);

	for (unsigned int i = 0U; i < 16U; i++)
		data[i + 41U] = m_reflector->m_name.at(i);

	for (unsigned int i = 0U; i < 3U; i++)
		data[i + 57U] = m_reflector->m_count.at(i);

	for (unsigned int i = 0U; i < 14U; i++)
		data[i + 70U] = m_reflector->m_desc.at(i);

	data[84U] = '0';
	data[85U] = '0';
	data[86U] = '0';
	data[87U] = '0';
	data[88U] = '0';

	data[89U] = 0x03U;			// End of data marker
	data[90U] = CCRC::addCRC(data, 90U);

	CUtils::dump(1U, "CONNECT Reply", data, 91U);

	createReply(data, 91U);

	m_seqNo++;
}

void CWiresX::sendDisconnectReply()
{
	unsigned char data[110U];
	::memset(data, 0x00U, 110U);
	::memset(data, ' ', 90U);

	data[0U] = m_seqNo;

	for (unsigned int i = 0U; i < 4U; i++)
		data[i + 1U] = DISC_RESP[i];

	for (unsigned int i = 0U; i < 5U; i++)
		data[i + 5U] = m_id.at(i);

	for (unsigned int i = 0U; i < 10U; i++)
		data[i + 10U] = m_node.at(i);

	for (unsigned int i = 0U; i < 14U; i++)
		data[i + 20U] = m_name.at(i);

	data[34U] = '1';
	data[35U] = '2';

	data[57U] = '0';
	data[58U] = '0';
	data[59U] = '0';

	data[89U] = 0x03U;			// End of data marker
	data[90U] = CCRC::addCRC(data, 90U);

	CUtils::dump(1U, "DISCONNECT Reply", data, 91U);

	createReply(data, 91U);

	m_seqNo++;
}

void CWiresX::sendAllReply()
{
	if (m_start == 0U)
		m_reflectors.reload();

	std::vector<CYSFReflector*>& curr = m_reflectors.current();

	unsigned char data[1100U];
	::memset(data, 0x00U, 1100U);

	data[0U] = m_seqNo;

	for (unsigned int i = 0U; i < 4U; i++)
		data[i + 1U] = ALL_RESP[i];

	data[5U] = '2';
	data[6U] = '1';

	for (unsigned int i = 0U; i < 5U; i++)
		data[i + 7U] = m_id.at(i);

	for (unsigned int i = 0U; i < 10U; i++)
		data[i + 12U] = m_node.at(i);

	unsigned int total = curr.size();
	if (total > 999U) total = 999U;

	unsigned int n = curr.size() - m_start;
	if (n > 20U) n = 20U;

	::sprintf((char*)(data + 22U), "%03u%03u", n, total);

	data[28U] = 0x0DU;

	unsigned int offset = 29U;
	for (unsigned int j = 0U; j < n; j++, offset += 50U) {
		CYSFReflector* refl = curr.at(j + m_start);

		::memset(data + offset, ' ', 50U);

		data[offset + 0U] = '5';

		for (unsigned int i = 0U; i < 5U; i++)
			data[i + offset + 1U] = refl->m_id.at(i);

		for (unsigned int i = 0U; i < 16U; i++)
			data[i + offset + 6U] = refl->m_name.at(i);

		for (unsigned int i = 0U; i < 3U; i++)
			data[i + offset + 22U] = refl->m_count.at(i);

		for (unsigned int i = 0U; i < 10U; i++)
			data[i + offset + 25U] = ' ';

		for (unsigned int i = 0U; i < 14U; i++)
			data[i + offset + 35U] = refl->m_desc.at(i);

		data[offset + 49U] = 0x0DU;
	}

	unsigned int k = 1029U - offset;
	for(unsigned int i = 0U; i < k; i++)
		data[i + offset] = 0x20U;

	offset += k;

	data[offset + 0U] = 0x03U;			// End of data marker
	data[offset + 1U] = CCRC::addCRC(data, offset + 1U);

	CUtils::dump(1U, "ALL Reply", data, offset + 2U);

	createReply(data, offset + 2U);

	m_seqNo++;
}

void CWiresX::sendSearchReply()
{
	if (m_search.size() == 0U) {
		sendSearchNotFoundReply();
		return;
	}

	std::vector<CYSFReflector*>& search = m_reflectors.search(m_search);
	if (search.size() == 0U) {
		sendSearchNotFoundReply();
		return;
	}

	unsigned char data[1100U];
	::memset(data, 0x00U, 1100U);

	data[0U] = m_seqNo;

	for (unsigned int i = 0U; i < 4U; i++)
		data[i + 1U] = ALL_RESP[i];

	data[5U] = '0';
	data[6U] = '2';

	for (unsigned int i = 0U; i < 5U; i++)
		data[i + 7U] = m_id.at(i);

	for (unsigned int i = 0U; i < 10U; i++)
		data[i + 12U] = m_node.at(i);

	data[22U] = '1';

	unsigned int total = search.size();
	if (total > 999U) total = 999U;

	unsigned int n = search.size() - m_start;
	if (n > 20U) n = 20U;

	::sprintf((char*)(data + 23U), "%02u%03u", n, total);

	data[28U] = 0x0DU;

	unsigned int offset = 29U;
	for (unsigned int j = 0U; j < n; j++, offset += 50U) {
		CYSFReflector* refl = search.at(j + m_start);

		::memset(data + offset, ' ', 50U);

		data[offset + 0U] = '1';

		for (unsigned int i = 0U; i < 5U; i++)
			data[i + offset + 1U] = refl->m_id.at(i);

		for (unsigned int i = 0U; i < 16U; i++)
			data[i + offset + 6U] = refl->m_name.at(i);

		for (unsigned int i = 0U; i < 3U; i++)
			data[i + offset + 22U] = refl->m_count.at(i);

		for (unsigned int i = 0U; i < 10U; i++)
			data[i + offset + 25U] = ' ';

		for (unsigned int i = 0U; i < 14U; i++)
			data[i + offset + 35U] = refl->m_desc.at(i);

		data[offset + 49U] = 0x0DU;
	}

	unsigned int k = 1029U - offset;
	for(unsigned int i = 0U; i < k; i++)
		data[i + offset] = 0x20U;

	offset += k;

	data[offset + 0U] = 0x03U;			// End of data marker
	data[offset + 1U] = CCRC::addCRC(data, offset + 1U);

	CUtils::dump(1U, "SEARCH Reply", data, offset + 2U);

	createReply(data, offset + 2U);

	m_seqNo++;
}

void CWiresX::sendSearchNotFoundReply()
{
	unsigned char data[70U];
	::memset(data, 0x00U, 70U);

	data[0U] = m_seqNo;

	for (unsigned int i = 0U; i < 4U; i++)
		data[i + 1U] = ALL_RESP[i];

	data[5U] = '0';
	data[6U] = '1';

	for (unsigned int i = 0U; i < 5U; i++)
		data[i + 7U] = m_id.at(i);

	for (unsigned int i = 0U; i < 10U; i++)
		data[i + 12U] = m_node.at(i);

	data[22U] = '1';
	data[23U] = '0';
	data[24U] = '0';
	data[25U] = '0';
	data[26U] = '0';
	data[27U] = '0';

	data[28U] = 0x0DU;

	data[29U] = 0x03U;			// End of data marker
	data[30U] = CCRC::addCRC(data, 30U);

	CUtils::dump(1U, "SEARCH Reply", data, 31U);

	createReply(data, 31U);

	m_seqNo++;
}

void CWiresX::sendCategoryReply()
{
	unsigned char data[1100U];
	::memset(data, 0x00U, 1100U);

	data[0U] = m_seqNo;

	for (unsigned int i = 0U; i < 4U; i++)
		data[i + 1U] = ALL_RESP[i];

	data[5U] = '2';
	data[6U] = '1';

	for (unsigned int i = 0U; i < 5U; i++)
		data[i + 7U] = m_id.at(i);

	for (unsigned int i = 0U; i < 10U; i++)
		data[i + 12U] = m_node.at(i);

	unsigned int n = m_category.size();
	if (n > 20U)
		n = 20U;

	::sprintf((char*)(data + 22U), "%03u%03u", n, n);

	data[28U] = 0x0DU;

	unsigned int offset = 29U;
	for (unsigned int j = 0U; j < n; j++, offset += 50U) {
		CYSFReflector* refl = m_category.at(j);

		::memset(data + offset, ' ', 50U);

		data[offset + 0U] = '5';

		for (unsigned int i = 0U; i < 5U; i++)
			data[i + offset + 1U] = refl->m_id.at(i);

		for (unsigned int i = 0U; i < 16U; i++)
			data[i + offset + 6U] = refl->m_name.at(i);

		for (unsigned int i = 0U; i < 3U; i++)
			data[i + offset + 22U] = refl->m_count.at(i);

		for (unsigned int i = 0U; i < 10U; i++)
			data[i + offset + 25U] = ' ';

		for (unsigned int i = 0U; i < 14U; i++)
			data[i + offset + 35U] = refl->m_desc.at(i);

		data[offset + 49U] = 0x0DU;
	}

	unsigned int k = 1029U - offset;
	for(unsigned int i = 0U; i < k; i++)
		data[i + offset] = 0x20U;

	offset += k;

	data[offset + 0U] = 0x03U;			// End of data marker
	data[offset + 1U] = CCRC::addCRC(data, offset + 1U);

	CUtils::dump(1U, "CATEGORY Reply", data, offset + 2U);

	createReply(data, offset + 2U);

	m_seqNo++;
}

bool CWiresX::isBusy() const
{
	return m_busy;
}
