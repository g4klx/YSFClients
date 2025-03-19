/*
 *   Copyright (C) 2009-2014,2016,2017,2018,2020,2025 by Jonathan Naylor G4KLX
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
#include "YSFFICH.h"
#include "Utils.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>


CIMRSNetwork::CIMRSNetwork() :
m_socket(IMRS_PORT),
m_dgIds(),
m_state(DGID_STATUS::NOTOPEN)
{
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
	if (ptr == nullptr)
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

	bool ret = m_socket.open();
	if (!ret) {
		m_state = DGID_STATUS::NOTOPEN;
		return false;
	} else {
		m_state = DGID_STATUS::NOTLINKED;
		return true;
	}
}

DGID_STATUS CIMRSNetwork::getStatus()
{
	return m_state;
}

void CIMRSNetwork::write(unsigned int dgId, const unsigned char* data)
{
	assert(data != nullptr);

	IMRSDGId* ptr = find(dgId);
	if (ptr == nullptr)
		return;

	CUtils::dump(1U, "YSF Data Received", data, 155U);

	CYSFFICH fich;
	fich.decode(data + 35U);

	switch (fich.getFI()) {
	case YSF_FI_HEADER:
	case YSF_FI_TERMINATOR:
		writeHeaderTrailer(ptr, fich, data);
		break;
	case YSF_FI_COMMUNICATIONS:
		writeData(ptr, fich, data);
		break;
	default:
		break;
	}
}

bool CIMRSNetwork::writeHeaderTrailer(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data)
{
	assert(ptr != nullptr);
	assert(data != nullptr);

	unsigned char buffer[200U];

	if (fich.getFI() == YSF_FI_HEADER) {
		buffer[0U] = 0x11U;
		ptr->m_seqNo = 0U;
	} else {
		buffer[0U] = 0x33U;
	}

	// Copy the sequence number LE (2 bytes)
	buffer[1U] = (ptr->m_seqNo << 0) & 0xFFU;
	buffer[2U] = (ptr->m_seqNo << 8) & 0xFFU;

	// Copy CSD1 and CSD2 (40 bytes)
	CYSFPayload payload;
	payload.readHeaderData(data + 35U, buffer + 7U);

	fich.getRaw(buffer + 3U);
	CUtils::dump(1U, "IMRS Network Data Sent", buffer, 47U);

	readHeaderTrailer(ptr, fich, buffer);

#ifdef notdef
	for (std::vector<IMRSDest*>::const_iterator it = ptr->m_destinations.begin(); it != ptr->m_destinations.end(); ++it) {
		// Set the correct DG-ID for this destination
		fich.setDGId((*it)->m_dgId);
		// Copy the raw FICH
		fich.getRaw(buffer + 3U);

		if (ptr->m_debug)
			CUtils::dump(1U, "IMRS Network Data Sent", buffer, 47U);

		m_socket.write(buffer, 47U, (*it)->m_addr, (*it)->m_addrLen);
	}
#endif

	ptr->m_seqNo++;

	return true;
}

bool CIMRSNetwork::writeData(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data)
{
	assert(ptr != nullptr);
	assert(data != nullptr);

	unsigned char buffer[200U];
	unsigned int length = 0U;

	buffer[0U] = 0x22U;

	// Copy the sequence number LE (2 bytes)
	buffer[1U] = (ptr->m_seqNo << 0) & 0xFFU;
	buffer[2U] = (ptr->m_seqNo << 8) & 0xFFU;

	unsigned char dt = fich.getDT();
	unsigned char ft = fich.getFT();
	unsigned char fn = fich.getFN();

	CYSFPayload payload;

	// Create the header
	switch (dt) {
	case YSF_DT_VD_MODE1:
		// Copy the DCH (20 bytes)
		payload.readVDMode1Data(data + 35U, buffer + 7U);
		// Copy the audio
		::memcpy(buffer + 27U + 0U,  data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 9U,  9U);
		::memcpy(buffer + 27U + 9U,  data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 27U, 9U);
		::memcpy(buffer + 27U + 18U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 45U, 9U);
		::memcpy(buffer + 27U + 27U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 63U, 9U);
		::memcpy(buffer + 27U + 36U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 81U, 9U);
		length = 72U;
		break;
	case YSF_DT_DATA_FR_MODE:
		// Copy the data
		::memcpy(buffer + 17U + 0U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 0U, 90U);
		length = 107U;
		break;
	case YSF_DT_VD_MODE2:
		// Copy the DCH (10 bytes)
		payload.readVDMode2Data(data + 35U, buffer + 7U);
		// Copy the audio
		::memcpy(buffer + 17U + 0U,  data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 5U,  13U);
		::memcpy(buffer + 17U + 13U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 23U, 13U);
		::memcpy(buffer + 17U + 26U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 41U, 13U);
		::memcpy(buffer + 17U + 39U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 59U, 13U);
		::memcpy(buffer + 17U + 52U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 77U, 13U);
		length = 82U;
		break;
	case YSF_DT_VOICE_FR_MODE:
		if (fn == 0U && ft == 1U) {
			// Copy the DCH (20 bytes)
			payload.readVoiceFRModeData(data + 35U, buffer + 7U);
			// Copy the audio
			::memcpy(buffer + 27U + 0U,  data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 54U, 18U);
			::memcpy(buffer + 27U + 18U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 72U, 18U);
			length = 107U;
		} else {
			// Copy the audio
			::memcpy(buffer + 17U + 0U,  data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 0U,  18U);
			::memcpy(buffer + 17U + 18U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 18U, 18U);
			::memcpy(buffer + 17U + 36U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 36U, 18U);
			::memcpy(buffer + 17U + 54U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 54U, 18U);
			::memcpy(buffer + 17U + 72U, data + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 72U, 18U);
			length = 107U;
		}
		break;
	default:
		return false;
	}

	fich.getRaw(buffer + 3U);
	CUtils::dump(1U, "IMRS Network Data Sent", buffer, length);

	readData(ptr, fich, buffer);

#ifdef notdef
	for (std::vector<IMRSDest*>::const_iterator it = ptr->m_destinations.begin(); it != ptr->m_destinations.end(); ++it) {
		// Set the correct DG-ID for this destination
		fich.setDGId((*it)->m_dgId);
		// Copy the raw FICH
		fich.getRaw(buffer + 3U);

		if (ptr->m_debug)
			CUtils::dump(1U, "IMRS Network Data Sent", buffer, length);

		m_socket.write(buffer, length, (*it)->m_addr, (*it)->m_addrLen);
	}
#endif

	ptr->m_seqNo++;

	return true;
}

void CIMRSNetwork::readHeaderTrailer(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data)
{
	assert(ptr != nullptr);
	assert(data != nullptr);

	unsigned char buffer[155U];

	::memcpy(buffer + 0U, "YSFD", 4U);

	unsigned char fi = fich.getFI();
	if (fi == YSF_FI_HEADER) {
		::memcpy(ptr->m_source, data + 17U + YSF_CALLSIGN_LENGTH, YSF_CALLSIGN_LENGTH);

		unsigned char cm = fich.getCM();
		if (cm == YSF_CM_GROUP1 || cm == YSF_CM_GROUP2)
			::memcpy(ptr->m_dest, "ALL       ", YSF_CALLSIGN_LENGTH);
		else
			::memcpy(ptr->m_dest, data + 17U + 0U, YSF_CALLSIGN_LENGTH);

		buffer[34U] = 0x00U;
	} else {
		uint16_t seqNo = (data[1U] << 0) + (data[2U] << 8);
		buffer[34U] = 0x01U | ((seqNo & 0x7FU) << 1);
	}

	::memcpy(buffer + 4U, "IMRS      ",   YSF_CALLSIGN_LENGTH);
	::memcpy(buffer + 14U, ptr->m_source, YSF_CALLSIGN_LENGTH);
	::memcpy(buffer + 24U, ptr->m_dest,   YSF_CALLSIGN_LENGTH);

	::memcpy(buffer + 35U, YSF_SYNC_BYTES, YSF_SYNC_LENGTH_BYTES);

	fich.encode(buffer + 35U);

	CYSFPayload payload;
	payload.writeHeaderData(data + 7U, buffer + 35U);

	CUtils::dump("YSF Data Transmitted", buffer, 155U);

#ifdef notdef
	unsigned char len = 155U;
	ptr->m_buffer.addData(&len, 1U);
	ptr->m_buffer.addData(buffer, len);
#endif
}

void CIMRSNetwork::readData(IMRSDGId* ptr, CYSFFICH& fich, const unsigned char* data)
{
	assert(ptr != nullptr);
	assert(data != nullptr);

	unsigned char buffer[155U];

	::memcpy(buffer + 0U, "YSFD", 4U);

	::memcpy(buffer + 4U,  "IMRS      ",  YSF_CALLSIGN_LENGTH);
	::memcpy(buffer + 14U, ptr->m_source, YSF_CALLSIGN_LENGTH);
	::memcpy(buffer + 24U, ptr->m_dest,   YSF_CALLSIGN_LENGTH);

	uint16_t seqNo = (data[1U] << 0) + (data[2U] << 8);
	buffer[34U] = (seqNo & 0x7FU) << 1;

	::memcpy(buffer + 35U, YSF_SYNC_BYTES, YSF_SYNC_LENGTH_BYTES);

	fich.encode(buffer + 35U);

	unsigned char dt = fich.getDT();
	unsigned char fn = fich.getFN();
	unsigned char ft = fich.getFT();

	CYSFPayload payload;

	// Create the header
	switch (dt) {
	case YSF_DT_VD_MODE1:
		// Copy the DCH
		payload.writeVDMode1Data(data + 7U, buffer + 35U);
		// Copy the audio
		::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 9U,  data + 27U + 0U,  9U);
		::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 27U, data + 27U + 9U,  9U);
		::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 45U, data + 27U + 18U, 9U);
		::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 63U, data + 27U + 27U, 9U);
		::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 81U, data + 27U + 36U, 9U);
		break;
	case YSF_DT_DATA_FR_MODE:
		// Copy the data
		::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 0U, data + 17U + 0U, 90U);
		break;
	case YSF_DT_VD_MODE2:
		// Copy the DCH
		payload.writeVDMode2Data(data + 7U, buffer + 35U);
		// Copy the audio
		::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 5U,  data + 17U + 0U,  13U);
		::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 23U, data + 17U + 13U, 13U);
		::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 41U, data + 17U + 26U, 13U);
		::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 59U, data + 17U + 39U, 13U);
		::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 77U, data + 17U + 52U, 13U);
		break;
	case YSF_DT_VOICE_FR_MODE:
		if (fn == 0U && ft == 1U) {
			// Copy the DCH
			payload.writeVoiceFRModeData(data + 7U, buffer + 35U);
			// nullptr the unused section
			::memset(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 45U, 0x00U, 9U);
			// Copy the audio
			::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 54U, data + 27U + 0U,  18U);
			::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 72U, data + 27U + 18U, 18U);
		} else {
			// Copy the audio
			::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 0U,  data + 17U + 0U,  18U);
			::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 18U, data + 17U + 18U, 18U);
			::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 36U, data + 17U + 36U, 18U);
			::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 54U, data + 17U + 54U, 18U);
			::memcpy(buffer + 35U + YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES + 72U, data + 17U + 72U, 18U);
		}
		break;
	default:
		return;
	}

	CUtils::dump("YSF Data Transmitted", buffer, 155U);

#ifdef notdef
	unsigned char len = 155U;
	ptr->m_buffer.addData(&len, 1U);
	ptr->m_buffer.addData(buffer, len);
#endif
}

void CIMRSNetwork::link()
{
}

void CIMRSNetwork::unlink()
{
}

void CIMRSNetwork::clock(unsigned int ms)
{
	unsigned char buffer[500U];

	sockaddr_storage addr;
	unsigned int addrLen;
	int length = m_socket.read(buffer, 500U, addr, addrLen);
	if (length <= 0)
		return;

	if (addrLen == 0U)
		return;

	CUtils::dump(1U, "IMRS Network Data Received", buffer, length);

	IMRSDGId* ptr = find(addr);
	if (ptr == nullptr)
		return;

	if (ptr->m_debug)
		CUtils::dump(1U, "IMRS Network Data Received", buffer, length);

	CYSFFICH fich;
	fich.setRaw(buffer + 3U);

	unsigned char fi = fich.getFI();
	switch (fi) {
	case YSF_FI_HEADER:
	case YSF_FI_TERMINATOR:
		readHeaderTrailer(ptr, fich, buffer);
		break;
	case YSF_FI_COMMUNICATIONS:
		readData(ptr, fich, buffer);
		break;
	default:
		break;
	}
}

unsigned int CIMRSNetwork::read(unsigned int dgId, unsigned char* data)
{
	assert(data != nullptr);

	IMRSDGId* ptr = find(dgId);
	if (ptr == nullptr)
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

	m_state = DGID_STATUS::NOTOPEN;
}

IMRSDGId* CIMRSNetwork::find(const sockaddr_storage& addr) const
{
	for (std::vector<IMRSDGId*>::const_iterator it1 = m_dgIds.begin(); it1 != m_dgIds.end(); ++it1) {
		for (std::vector<IMRSDest*>::const_iterator it2 = (*it1)->m_destinations.begin(); it2 != (*it1)->m_destinations.end(); ++it2) {
			if (CUDPSocket::match(addr, (*it2)->m_addr))
				return *it1;
		}
	}

	return nullptr;
}

IMRSDGId* CIMRSNetwork::find(unsigned int dgId) const
{
	for (std::vector<IMRSDGId*>::const_iterator it = m_dgIds.begin(); it != m_dgIds.end(); ++it) {
		if (dgId == (*it)->m_dgId)
			return *it;
	}

	return nullptr;
}
