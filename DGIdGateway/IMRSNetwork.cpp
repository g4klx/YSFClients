/*
 *   Copyright (C) 2009-2014,2016,2017,2018,2020,2021,2024 by Jonathan Naylor G4KLX
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

#include "IMRSNetwork.h"
#include "YSFPayload.h"
#include "YSFDefines.h"
#include "Utils.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>

static const unsigned char PING[]        = {0x00U, 0x00U, 0x07U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
static const unsigned char CONNECT[]     = {0x00U, 0x2CU, 0x08U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x04U, 0x00U, 0x00U};
static const unsigned char HEADER1[]     = {0x00U, 0x4BU, 0x00U, 0x00U, 0x00U, 0x00U, 0x07U};
static const unsigned char HEADER2[]     = {0x00U, 0x00U, 0x00U, 0x00U, 0x49U, 0x2AU, 0x2AU};
static const unsigned char DATA1[]       = {0x00U, 0xA5U, 0x00U, 0x00U, 0x00U, 0x00U, 0x07U};
static const unsigned char DATA2[]       = {0x00U, 0x00U, 0x00U, 0x00U, 0x32U, 0x2AU, 0x2AU};
static const unsigned char TERMINATOR1[] = {0x00U, 0x0FU, 0x00U, 0x00U, 0x00U, 0x00U, 0x07U};
static const unsigned char TERMINATOR2[] = {0x00U, 0x00U, 0x00U, 0x00U, 0x54U, 0x2AU, 0x2AU};

static const char* DATA3 = "2A2A2A2A2A4835215245";

static const char* DUMMY_ID = "G0gBJ";


CIMRSNetwork::CIMRSNetwork(const std::string& callsign) :
m_socket(IMRS_PORT),
m_callsign(callsign),
m_dgIds(),
m_random(),
m_streamId(0U)
{
	m_callsign.resize(YSF_CALLSIGN_LENGTH, ' ');

	std::random_device rd;
	std::mt19937 mt(rd());
	m_random = mt;
}

CIMRSNetwork::~CIMRSNetwork()
{
}

void CIMRSNetwork::addDGId(unsigned int dgId, const std::string& name, const std::vector<IMRSDest*>& destinations, bool debug)
{
	IMRSDGId* f = new IMRSDGId;
	f->m_dgId         = dgId;
	f->m_name         = name;
	f->m_seqNo        = 0U;
	f->m_destinations = destinations;
	f->m_debug        = debug;

	m_dgIds.push_back(f);
}

std::string CIMRSNetwork::getDesc(unsigned int dgId)
{
	IMRSDGId* ptr = find(dgId);
	if (ptr == NULL)
		return "IMRS: Unknown";

	return "IMRS: " + ptr->m_name;
}

unsigned int CIMRSNetwork::getDGId()
{
	return 0U;
}

bool CIMRSNetwork::open()
{
	LogMessage("Opening IMRS network connection");

	return m_socket.open();
}

DGID_STATUS CIMRSNetwork::getStatus()
{
	for (std::vector<IMRSDGId*>::const_iterator it1 = m_dgIds.cbegin(); it1 != m_dgIds.cend(); ++it1) {
		std::vector<IMRSDest*> dests = (*it1)->m_destinations;
		for (std::vector<IMRSDest*>::const_iterator it2 = dests.cbegin(); it2 != dests.cend(); ++it2) {
			IMRSDest* dest = *it2;
			if (dest->m_state == DS_LINKED)
				return DS_LINKED;
		}
	}

	return DS_NOTLINKED;
}

void CIMRSNetwork::write(unsigned int dgId, CYSFFICH& fich, const unsigned char* data)
{
	assert(data != NULL);

	IMRSDGId* ptr = find(dgId);
	if (ptr == NULL)
		return;

	// CUtils::dump(1U, "YSF Data Received", data, 155U);
	CUtils::dump("YSF Data Received from main", data, 155U);

	switch (fich.getFI()) {
	case YSF_FI_HEADER:
		writeHeader(ptr, fich, data);
		break;
	case YSF_FI_TERMINATOR:
		writeTerminator(ptr, fich, data);
		break;
	case YSF_FI_COMMUNICATIONS:
		writeData(ptr, fich, data);
		break;
	default:
		break;
	}
}

bool CIMRSNetwork::writeHeader(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data)
{
	assert(ptr != NULL);
	assert(data != NULL);

	// Create a random id for this transmission
	std::uniform_int_distribution<uint16_t> dist(0x0001, 0xFFFE);
	m_streamId = dist(m_random);

	ptr->m_seqNo = 0U;

	unsigned char buffer[100U];

	::memset(buffer, 0x00U, 100U);

	::memcpy(buffer + 0U, HEADER1, 7U);

	// Set the time in milliseconds
	uint32_t time = ptr->m_seqNo * 100U;
	buffer[7U] = (time >> 16) & 0xFFU;
	buffer[8U] = (time >> 8) & 0xFFU;
	buffer[9U] = (time >> 0) & 0xFFU;

	// Set the stream id
	buffer[10U] = (m_streamId >> 8) & 0xFFU;
	buffer[11U] = (m_streamId >> 0) & 0xFFU;

	::memcpy(buffer + 12U, HEADER2, 7U);

	// Set the sequence number in ASCII hex
	CUtils::toHex(buffer + 19U, (unsigned char*)&ptr->m_seqNo, 2U);

	// Will contain CSD1 and CSD2
	unsigned char dat[4U * YSF_CALLSIGN_LENGTH];

	CYSFPayload payload;
	payload.readHeaderData(data + 35U, dat);

	unsigned char cm = fich.getCM();

	// Set the Destination Radio Id and Source Radio Id
	if (cm != YSF_CM_RADIO_ID) {
		::memcpy(buffer + 31U, "*****", YSF_RADIO_ID_LENGTH);
		::memcpy(buffer + 36U, DUMMY_ID, YSF_RADIO_ID_LENGTH);
	} else {
		::memcpy(buffer + 31U, dat + 0U, YSF_RADIO_ID_LENGTH);
		::memcpy(buffer + 36U, dat + 5U, YSF_RADIO_ID_LENGTH);
	}

	// Set the Source Callsign
	::memcpy(buffer + 41U, dat + 10U, YSF_CALLSIGN_LENGTH);

	// Set the Downlink Callsign
	::memcpy(buffer + 51U, dat + 20U, YSF_CALLSIGN_LENGTH);

	// Set the Uplink Callsign
	::memcpy(buffer + 61U, buffer + 4U, YSF_CALLSIGN_LENGTH);

	// Set the Downlink Radio Id
	::memcpy(buffer + 66U, "     ", YSF_RADIO_ID_LENGTH);

	// Set the Uplink Radio Id
	::memcpy(buffer + 71U, "     ", YSF_RADIO_ID_LENGTH);

	// Set the VOIP Station Id
	::memcpy(buffer + 76U, "     ", YSF_RADIO_ID_LENGTH);

	// Set unknown Radio Id
	::memcpy(buffer + 81U, "     ", YSF_RADIO_ID_LENGTH);

	// Set the Transmission Source Radio Id
	::memcpy(buffer + 86U, DUMMY_ID, YSF_RADIO_ID_LENGTH);

	for (std::vector<IMRSDest*>::const_iterator it = ptr->m_destinations.cbegin(); it != ptr->m_destinations.cend(); ++it) {
		// if (ptr->m_debug)
		//	CUtils::dump(1U, "IMRS Network Header Sent", buffer, 91U);
		CUtils::dump("IMRS Network Header Sent", buffer, 91U);

		if ((*it)->m_state == DS_LINKED) {
			// Set the correct DG-ID for this destination
			fich.setDGId((*it)->m_dgId);

			// Set the new FICH in ASCII hex
			fich.getASCII(buffer + 23U);

			m_socket.write(buffer, 91U, (*it)->m_addr, (*it)->m_addrLen);
		}
	}

	ptr->m_seqNo++;

	return true;
}

bool CIMRSNetwork::writeData(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data)
{
	assert(ptr != NULL);
	assert(data != NULL);

	unsigned char buffer[200U];

	::memset(buffer, 0x00U, 200U);

	::memcpy(buffer + 0U, DATA1, 7U);

	// Set the time in milliseconds
	uint32_t time = ptr->m_seqNo * 100U;
	buffer[7U] = (time >> 16) & 0xFFU;
	buffer[8U] = (time >> 8) & 0xFFU;
	buffer[9U] = (time >> 0) & 0xFFU;

	// Set the stream id
	buffer[10U] = (m_streamId >> 8) & 0xFFU;
	buffer[11U] = (m_streamId >> 0) & 0xFFU;

	::memcpy(buffer + 12U, DATA2, 7U);

	// Set the sequence number in ASCII hex
	CUtils::toHex(buffer + 19U, (unsigned char*)&ptr->m_seqNo, 2U);

	::memcpy(buffer + 31U, DATA3, 20U);

	CYSFPayload payload;

	unsigned int length = 0U;

	unsigned char dt = fich.getDT();
	unsigned char ft = fich.getFT();
	unsigned char fn = fich.getFN();

	switch (dt) {
	case YSF_DT_DATA_FR_MODE:
		// Copy the data as ASCII hex
		CUtils::toHex(buffer + 51U + 0U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 0U, 90U);
		length = 231U;
		break;
	case YSF_DT_VD_MODE2:
		// Copy the audio as ASCII hex
		CUtils::toHex(buffer + 51U + 0U,   data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 5U,  13U);
		CUtils::toHex(buffer + 51U + 26U,  data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 23U, 13U);
		CUtils::toHex(buffer + 51U + 52U,  data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 41U, 13U);
		CUtils::toHex(buffer + 51U + 78U,  data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 59U, 13U);
		CUtils::toHex(buffer + 51U + 104U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 77U, 13U);
		length = 181U;
		break;
	case YSF_DT_VOICE_FR_MODE:
		if (fn == 0U && ft == 1U) {
			// Copy the audio as ASCII hex
			CUtils::toHex(buffer + 51U + 0U,  data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 54U, 18U);
			CUtils::toHex(buffer + 51U + 36U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 72U, 18U);
			length = 123U;
		} else {
			// Copy the audio as ASCII hex
			CUtils::toHex(buffer + 51U + 0U,   data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 0U,  18U);
			CUtils::toHex(buffer + 51U + 36U,  data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 18U, 18U);
			CUtils::toHex(buffer + 51U + 72U,  data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 36U, 18U);
			CUtils::toHex(buffer + 51U + 108U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 54U, 18U);
			CUtils::toHex(buffer + 51U + 144U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 72U, 18U);
			length = 231U;
		}
		break;
	default:
		return false;
	}

	buffer[1U] = length - 16U;

	for (std::vector<IMRSDest*>::const_iterator it = ptr->m_destinations.cbegin(); it != ptr->m_destinations.cend(); ++it) {
		// if (ptr->m_debug)
		//	CUtils::dump(1U, "IMRS Network Data Sent", buffer, length);
		CUtils::dump("IMRS Network Data Sent", buffer, length);

		if ((*it)->m_state == DS_LINKED) {
			// Set the correct DG-ID for this destination
			fich.setDGId((*it)->m_dgId);

			// Set the new FICH in ASCII hex
			fich.getASCII(buffer + 23U);

			m_socket.write(buffer, length, (*it)->m_addr, (*it)->m_addrLen);
		}
	}

	ptr->m_seqNo++;

	return true;
}

bool CIMRSNetwork::writeTerminator(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data)
{
	assert(ptr != NULL);
	assert(data != NULL);

	unsigned char buffer[40U];

	::memset(buffer, 0x00U, 40U);

	::memcpy(buffer + 0U, TERMINATOR1, 7U);

	// Set the time in milliseconds
	uint32_t time = ptr->m_seqNo * 100U;
	buffer[7U] = (time >> 16) & 0xFFU;
	buffer[8U] = (time >> 8) & 0xFFU;
	buffer[9U] = (time >> 0) & 0xFFU;

	// Set the stream id
	buffer[10U] = (m_streamId >> 8) & 0xFFU;
	buffer[11U] = (m_streamId >> 0) & 0xFFU;

	::memcpy(buffer + 12U, TERMINATOR2, 7U);

	// Set the sequence number in ASCII hex
	CUtils::toHex(buffer + 19U, (unsigned char*)&ptr->m_seqNo, 2U);

	for (std::vector<IMRSDest*>::const_iterator it = ptr->m_destinations.cbegin(); it != ptr->m_destinations.cend(); ++it) {
		// if (ptr->m_debug)
		//	CUtils::dump(1U, "IMRS Network Terminator Sent", buffer, 31U);
		CUtils::dump("IMRS Network Terminator Sent", buffer, 31U);

		if ((*it)->m_state == DS_LINKED) {
			// Set the correct DG-ID for this destination
			fich.setDGId((*it)->m_dgId);

			// Set the new FICH in ASCII hex
			fich.getASCII(buffer + 23U);

			m_socket.write(buffer, 31U, (*it)->m_addr, (*it)->m_addrLen);
		}
	}

	return true;
}

void CIMRSNetwork::readHeader(IMRSDGId* ptr, const unsigned char* data)
{
	assert(ptr != NULL);
	assert(data != NULL);

	unsigned char buffer[155U];

	::memcpy(buffer + 0U, "YSFD", 4U);

	::memcpy(ptr->m_origin, data + 61U, YSF_CALLSIGN_LENGTH);
	::memcpy(ptr->m_source, data + 41U, YSF_CALLSIGN_LENGTH);

	// Get the FICH from ASCII hex
	CYSFFICH fich;
	fich.setASCII(data + 23U);
	fich.encode(buffer + 35U);

	unsigned char cm = fich.getCM();
	if (cm == YSF_CM_GROUP1 || cm == YSF_CM_RADIO_ID)
		::memcpy(ptr->m_dest, "ALL       ", YSF_CALLSIGN_LENGTH);
	else
		::memcpy(ptr->m_dest, data + 51U, YSF_CALLSIGN_LENGTH);

	buffer[34U] = (data[11U] & 0x7FU) << 1;

	::memcpy(buffer + 4U,  ptr->m_origin, YSF_CALLSIGN_LENGTH);
	::memcpy(buffer + 14U, ptr->m_source, YSF_CALLSIGN_LENGTH);
	::memcpy(buffer + 24U, ptr->m_dest,   YSF_CALLSIGN_LENGTH);

	::memcpy(buffer + 35U, YSF_SYNC_BYTES, YSF_SYNC_LENGTH_BYTES);

	if (cm != YSF_CM_RADIO_ID) {
		::memcpy(ptr->m_csd1 + 0U, "**********", YSF_CALLSIGN_LENGTH);
	} else {
		::memcpy(ptr->m_csd1 + 0U, buffer + 31U, YSF_RADIO_ID_LENGTH);
		::memcpy(ptr->m_csd1 + 5U, buffer + 36U, YSF_RADIO_ID_LENGTH);
	}

	// Set the Source Callsign
	::memcpy(ptr->m_csd1 + 10U, buffer + 41U, YSF_CALLSIGN_LENGTH);

	// Set the Downlink Callsign
	::memcpy(ptr->m_csd2 + 0U,  buffer + 51U, YSF_CALLSIGN_LENGTH);

	// Set the Uplink Callsign
	::memcpy(ptr->m_csd2 + 10U, buffer + 61U, YSF_CALLSIGN_LENGTH);

	unsigned char dat[4U * YSF_CALLSIGN_LENGTH];
	::memcpy(dat + 0U,  ptr->m_csd1, 2U * YSF_CALLSIGN_LENGTH);
	::memcpy(dat + 20U, ptr->m_csd2, 2U * YSF_CALLSIGN_LENGTH);

	CYSFPayload payload;
	payload.writeHeaderData(dat, buffer + 35U);

	CUtils::dump("YSF Data Converted", buffer, 155U);

	unsigned char len = 155U;
	ptr->m_buffer.addData(&len, 1U);
	ptr->m_buffer.addData(buffer, len);
}

void CIMRSNetwork::readData(IMRSDGId* ptr, const unsigned char* data)
{
	assert(ptr != NULL);
	assert(data != NULL);

	unsigned char buffer[155U];

	::memcpy(buffer + 0U, "YSFD", 4U);

	::memcpy(buffer + 4U,  ptr->m_origin, YSF_CALLSIGN_LENGTH);
	::memcpy(buffer + 14U, ptr->m_source, YSF_CALLSIGN_LENGTH);
	::memcpy(buffer + 24U, ptr->m_dest,   YSF_CALLSIGN_LENGTH);

	buffer[34U] = (data[11U] & 0x7FU) << 1;

	::memcpy(buffer + 35U, YSF_SYNC_BYTES, YSF_SYNC_LENGTH_BYTES);

	// Get the FICH from ASCII hex
	CYSFFICH fich;
	fich.setASCII(data + 23U);
	fich.encode(buffer + 35U);

	unsigned char dt = fich.getDT();
	unsigned char fn = fich.getFN();
	unsigned char ft = fich.getFT();

	CYSFPayload payload;

	// Create the header
	switch (dt) {
	case YSF_DT_DATA_FR_MODE:
		// Copy the DCH
		if (fn == 0U) {
			unsigned char dat[4U * YSF_CALLSIGN_LENGTH];
			::memcpy(dat + 0U,  ptr->m_csd1, 2U * YSF_CALLSIGN_LENGTH);
			::memcpy(dat + 20U, ptr->m_csd2, 2U * YSF_CALLSIGN_LENGTH);
			payload.writeVDMode2Data(dat, buffer + 35U);
		} else {
			payload.writeVDMode2Data((unsigned char*)"                                        ", buffer + 35U);
		}

		// Copy the data
		CUtils::fromHex(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 0U, data + 51U + 0U, 180U);
		break;

	case YSF_DT_VD_MODE2:
		// Copy the DCH
		switch (fn) {
		case 0U:
			payload.writeVDMode2Data(ptr->m_csd1 + 0U, buffer + 35U);
			break;
		case 1U:
			payload.writeVDMode2Data(ptr->m_csd1 + 10U, buffer + 35U);
			break;
		case 2U:
			payload.writeVDMode2Data(ptr->m_csd2 + 0U, buffer + 35U);
			break;
		case 3U:
			payload.writeVDMode2Data(ptr->m_csd2 + 10U, buffer + 35U);
			break;
		default:
			payload.writeVDMode2Data((unsigned char*)"          ", buffer + 35U);
			break;
		}

		// Copy the audio
		CUtils::fromHex(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 5U,  data + 51U + 0U,   26U);
		CUtils::fromHex(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 23U, data + 51U + 26U,  26U);
		CUtils::fromHex(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 41U, data + 51U + 52U,  26U);
		CUtils::fromHex(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 59U, data + 51U + 78U,  26U);
		CUtils::fromHex(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 77U, data + 51U + 104U, 26U);
		break;

	case YSF_DT_VOICE_FR_MODE:
		if (fn == 0U && ft == 1U) {
			// Copy the DCH
			payload.writeVoiceFRModeData((unsigned char*)"                    ", buffer + 35U);

			// NULL the unused section
			::memset(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 45U, 0x00U, 9U);

			// Copy the audio
			CUtils::fromHex(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 54U, data + 51U + 0U,  36U);
			CUtils::fromHex(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 72U, data + 51U + 36U, 36U);
		} else {
			// Copy the audio
			CUtils::fromHex(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 0U,  data + 51U + 0U,   36U);
			CUtils::fromHex(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 18U, data + 51U + 36U,  36U);
			CUtils::fromHex(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 36U, data + 51U + 72U,  36U);
			CUtils::fromHex(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 54U, data + 51U + 108U, 36U);
			CUtils::fromHex(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 72U, data + 51U + 144U, 36U);
		}
		break;

	default:
		return;
	}

	CUtils::dump("YSF Data Converted", buffer, 155U);

	unsigned char len = 155U;
	ptr->m_buffer.addData(&len, 1U);
	ptr->m_buffer.addData(buffer, len);
}

void CIMRSNetwork::readTerminator(IMRSDGId* ptr, const unsigned char* data)
{
	assert(ptr != NULL);
	assert(data != NULL);

	unsigned char buffer[155U];

	::memcpy(buffer + 0U, "YSFD", 4U);

	buffer[34U] = 0x01U | ((data[11U] & 0x7FU) << 1);

	::memcpy(buffer + 4U,  ptr->m_origin, YSF_CALLSIGN_LENGTH);
	::memcpy(buffer + 14U, ptr->m_source, YSF_CALLSIGN_LENGTH);
	::memcpy(buffer + 24U, ptr->m_dest,   YSF_CALLSIGN_LENGTH);

	::memcpy(buffer + 35U, YSF_SYNC_BYTES, YSF_SYNC_LENGTH_BYTES);

	// Get the FICH from ASCII hex
	CYSFFICH fich;
	fich.setASCII(data + 23U);
	fich.encode(buffer + 35U);

	CYSFPayload payload;
	payload.writeHeaderData(data + 7U, buffer + 35U);

	CUtils::dump("YSF Data Converted", buffer, 155U);

	unsigned char len = 155U;
	ptr->m_buffer.addData(&len, 1U);
	ptr->m_buffer.addData(buffer, len);
}

void CIMRSNetwork::link()
{
	for (std::vector<IMRSDGId*>::const_iterator it1 = m_dgIds.cbegin(); it1 != m_dgIds.cend(); ++it1) {
		std::vector<IMRSDest*> dests = (*it1)->m_destinations;
		bool debug = (*it1)->m_debug;
		for (std::vector<IMRSDest*>::const_iterator it2 = dests.cbegin(); it2 != dests.cend(); ++it2) {
			IMRSDest* dest = *it2;
			dest->m_state = DS_LINKING;
			dest->m_timer.start();
			writePongConnect(*dest, debug);
		}
	}
}

void CIMRSNetwork::unlink()
{
	for (std::vector<IMRSDGId*>::const_iterator it1 = m_dgIds.cbegin(); it1 != m_dgIds.cend(); ++it1) {
		std::vector<IMRSDest*> dests = (*it1)->m_destinations;
		for (std::vector<IMRSDest*>::const_iterator it2 = dests.cbegin(); it2 != dests.cend(); ++it2) {
			IMRSDest* dest = *it2;
			dest->m_state = DS_NOTLINKED;
			dest->m_timer.stop();
		}
	}
}

void CIMRSNetwork::clock(unsigned int ms)
{
	for (std::vector<IMRSDGId*>::const_iterator it1 = m_dgIds.cbegin(); it1 != m_dgIds.cend(); ++it1) {
		std::vector<IMRSDest*> dests = (*it1)->m_destinations;
		bool debug = (*it1)->m_debug;
		for (std::vector<IMRSDest*>::const_iterator it2 = dests.cbegin(); it2 != dests.cend(); ++it2) {
			IMRSDest* dest = *it2;
			switch (dest->m_state) {
			case DS_LINKING:
				dest->m_timer.clock(ms);
				if (dest->m_timer.isRunning() && dest->m_timer.hasExpired()) {
					writePongConnect(*dest, debug);
					dest->m_timer.start();
				}
				break;

			case DS_LINKED:
				dest->m_timer.clock(ms);
				if (dest->m_timer.isRunning() && dest->m_timer.hasExpired()) {
					writePing(*dest, debug);
					dest->m_timer.start();
				}
				break;

			default:
				break;
			}
		}
	}

	unsigned char buffer[500U];

	sockaddr_storage addr;
	unsigned int addrLen;
	int length = m_socket.read(buffer, 500U, addr, addrLen);
	if (length <= 0)
		return;

	if (addrLen == 0U)
		return;

	IMRSDGId* ptr  = NULL;
	IMRSDest* dest = NULL;

	bool ret = find(addr, ptr, dest);
	if (!ret)
		return;

	// if (ptr->m_debug)
	//	CUtils::dump(1U, "IMRS Network Data Received", buffer, length);
	CUtils::dump("IMRS Network Data Received", buffer, length);

	if (dest->m_state != DS_LINKED) {
		char text[50U];
		CUDPSocket::display(addr, text, 50U);

		if (dest->m_state == DS_LINKING)
			LogMessage("Established IMRS link to %s", text);
		else
			LogMessage("Incoming IMRS link from %s", text);

		dest->m_state = DS_LINKED;
		dest->m_timer.start();
	}

	switch (length) {
	case 16:	// PING received
		LogMessage("Ping received");
		writePongConnect(*dest, ptr->m_debug);
		break;
	case 31:	// TERMINATOR received
		LogMessage("Terminator received");
		readTerminator(ptr, buffer);
		break;
	case 60:	// PONG/CONNECT received
		LogMessage("Pong/Connect received");
		break;
	case 91:	// HEADER received
		LogMessage("Header received");
		readHeader(ptr, buffer);
		break;
	case 181:	// DATA received
	case 231:	// ???
		LogMessage("Data received");
		readData(ptr, buffer);
		break;
	default:
		LogWarning("Unknown IMRS packet received");
		break;
	}
}

unsigned int CIMRSNetwork::read(unsigned int dgId, unsigned char* data)
{
	assert(data != NULL);

	IMRSDGId* ptr = find(dgId);
	if (ptr == NULL)
		return 0U;

	if (ptr->m_buffer.isEmpty())
		return 0U;

	unsigned char len = 0U;
	ptr->m_buffer.getData(&len, 1U);

	ptr->m_buffer.getData(data, len);

	return len;
}

void CIMRSNetwork::close()
{
	LogMessage("Closing IMRS network connection");

	m_socket.close();
}

bool CIMRSNetwork::find(const sockaddr_storage& addr, IMRSDGId*& ptr, IMRSDest*& dest) const
{
	for (std::vector<IMRSDGId*>::const_iterator it1 = m_dgIds.cbegin(); it1 != m_dgIds.cend(); ++it1) {
		for (std::vector<IMRSDest*>::const_iterator it2 = (*it1)->m_destinations.cbegin(); it2 != (*it1)->m_destinations.cend(); ++it2) {
			if (CUDPSocket::match(addr, (*it2)->m_addr)) {
				ptr  = *it1;
				dest = *it2;
				return true;
			}
		}
	}

	return false;
}

IMRSDGId* CIMRSNetwork::find(unsigned int dgId) const
{
	for (std::vector<IMRSDGId*>::const_iterator it = m_dgIds.cbegin(); it != m_dgIds.cend(); ++it) {
		if (dgId == (*it)->m_dgId)
			return *it;
	}

	return NULL;
}

bool CIMRSNetwork::writePongConnect(const IMRSDest& dest, bool debug)
{
	unsigned char buffer[60U];

	::memset(buffer, 0x00U, 60U);
	::memcpy(buffer + 0U, CONNECT, 20U);

	// A dummy MAC address
	::memcpy(buffer + 20U, "\x01\x02\x03\x04\x05\x06", 6U);

	// The callsign of the gateway/repeater/hotspot
	::memcpy(buffer + 26U, m_callsign.c_str(), YSF_CALLSIGN_LENGTH);

	// A dummy radio id
	::memcpy(buffer + 36U, DUMMY_ID, YSF_RADIO_ID_LENGTH);

	if (debug)
		CUtils::dump(1U, "IMRS Connect Sent", buffer, 60U);

	return m_socket.write(buffer, 60U, dest.m_addr, dest.m_addrLen);
}

bool CIMRSNetwork::writePing(const IMRSDest& dest, bool debug)
{
	if (debug)
		CUtils::dump(1U, "IMRS Ping Sent", PING, 16U);
	 
	return m_socket.write(PING, 16U, dest.m_addr, dest.m_addrLen);
}