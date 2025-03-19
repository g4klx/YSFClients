/*
 *   Copyright (C) 2012,2013,2015,2017,2018,2025 by Jonathan Naylor G4KLX
 *   Copyright (C) 2011 by DV Developer Group. DJ0ABR
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

#include "DTMF.h"

#include "Utils.h"

const unsigned char DTMF_VD2_MASK[] = { 0xCCU, 0xCCU, 0xDDU, 0xDDU, 0xEEU, 0xEEU, 0xFFU, 0xFFU, 0xEEU, 0xEEU, 0xDDU, 0x99U, 0x98U };
const unsigned char DTMF_VD2_SIG[]  = { 0x08U, 0x80U, 0xC9U, 0x10U, 0x26U, 0xA0U, 0xE3U, 0x31U, 0xE2U, 0xE6U, 0xD5U, 0x08U, 0x88U };

const unsigned char DTMF_VD2_SYM_MASK[] = { 0x33U, 0x33U, 0x22U, 0x22U, 0x11U, 0x11U, 0x11U, 0x11U, 0x22U, 0x66U, 0x66U };
const unsigned char DTMF_VD2_SYM0[] = { 0x33U, 0x11U, 0x22U, 0x02U, 0x00U, 0x00U, 0x01U, 0x11U, 0x00U, 0x04U, 0x62U };
const unsigned char DTMF_VD2_SYM1[] = { 0x33U, 0x10U, 0x20U, 0x20U, 0x00U, 0x01U, 0x01U, 0x10U, 0x00U, 0x04U, 0x62U };
const unsigned char DTMF_VD2_SYM2[] = { 0x22U, 0x23U, 0x02U, 0x02U, 0x00U, 0x10U, 0x01U, 0x01U, 0x00U, 0x04U, 0x62U };
const unsigned char DTMF_VD2_SYM3[] = { 0x22U, 0x22U, 0x00U, 0x20U, 0x00U, 0x11U, 0x01U, 0x00U, 0x00U, 0x04U, 0x62U };
const unsigned char DTMF_VD2_SYM4[] = { 0x11U, 0x11U, 0x22U, 0x02U, 0x01U, 0x00U, 0x00U, 0x11U, 0x00U, 0x06U, 0x44U };
const unsigned char DTMF_VD2_SYM5[] = { 0x11U, 0x10U, 0x20U, 0x20U, 0x01U, 0x01U, 0x00U, 0x10U, 0x00U, 0x06U, 0x44U };
const unsigned char DTMF_VD2_SYM6[] = { 0x00U, 0x23U, 0x02U, 0x02U, 0x01U, 0x10U, 0x00U, 0x01U, 0x00U, 0x06U, 0x44U };
const unsigned char DTMF_VD2_SYM7[] = { 0x00U, 0x22U, 0x00U, 0x20U, 0x01U, 0x11U, 0x00U, 0x00U, 0x00U, 0x06U, 0x44U };
const unsigned char DTMF_VD2_SYM8[] = { 0x33U, 0x11U, 0x22U, 0x02U, 0x10U, 0x00U, 0x11U, 0x11U, 0x22U, 0x60U, 0x22U };
const unsigned char DTMF_VD2_SYM9[] = { 0x33U, 0x10U, 0x20U, 0x20U, 0x10U, 0x01U, 0x11U, 0x10U, 0x22U, 0x60U, 0x22U };
const unsigned char DTMF_VD2_SYMA[] = { 0x22U, 0x23U, 0x02U, 0x02U, 0x10U, 0x10U, 0x11U, 0x01U, 0x22U, 0x60U, 0x22U };
const unsigned char DTMF_VD2_SYMB[] = { 0x22U, 0x22U, 0x00U, 0x20U, 0x10U, 0x11U, 0x11U, 0x00U, 0x22U, 0x60U, 0x22U };
const unsigned char DTMF_VD2_SYMC[] = { 0x11U, 0x11U, 0x22U, 0x02U, 0x11U, 0x00U, 0x10U, 0x11U, 0x22U, 0x62U, 0x04U };
const unsigned char DTMF_VD2_SYMD[] = { 0x11U, 0x10U, 0x20U, 0x20U, 0x11U, 0x01U, 0x10U, 0x10U, 0x22U, 0x62U, 0x04U };
const unsigned char DTMF_VD2_SYMS[] = { 0x00U, 0x23U, 0x02U, 0x02U, 0x11U, 0x10U, 0x10U, 0x01U, 0x22U, 0x62U, 0x04U };
const unsigned char DTMF_VD2_SYMH[] = { 0x00U, 0x22U, 0x00U, 0x20U, 0x11U, 0x11U, 0x10U, 0x00U, 0x22U, 0x62U, 0x04U };

const unsigned char VD2_SILENCE[] = { 0x7BU, 0xB2U, 0x8EU, 0x43U, 0x36U, 0xE4U, 0xA2U, 0x39U, 0x78U, 0x49U, 0x33U, 0x68U, 0x33U };

CDTMF::CDTMF() :
m_data(),
m_command(),
m_pressed(false),
m_releaseCount(0U),
m_pressCount(0U),
m_lastChar(' ')
{
}

CDTMF::~CDTMF()
{
}

WX_STATUS CDTMF::decodeVDMode2(unsigned char* payload, bool end)
{
	assert(payload != nullptr);

	payload += YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES;

	for (unsigned int offset = 5U; offset < 90U; offset += 18U) {
		WX_STATUS status = decodeVDMode2Slice(payload + offset, end);
		if (status != WX_STATUS::NONE)
			return status;
	}

	return WX_STATUS::NONE;
}

WX_STATUS CDTMF::decodeVDMode2Slice(unsigned char* ambe, bool end)
{
	// DTMF begins with these byte values
	if (!end && (ambe[0] & DTMF_VD2_MASK[0]) == DTMF_VD2_SIG[0] && (ambe[1] & DTMF_VD2_MASK[1]) == DTMF_VD2_SIG[1] &&
		(ambe[2] & DTMF_VD2_MASK[2])   == DTMF_VD2_SIG[2]  && (ambe[3] & DTMF_VD2_MASK[3])   == DTMF_VD2_SIG[3] &&
		(ambe[4] & DTMF_VD2_MASK[4])   == DTMF_VD2_SIG[4]  && (ambe[5] & DTMF_VD2_MASK[5])   == DTMF_VD2_SIG[5] &&
		(ambe[6] & DTMF_VD2_MASK[6])   == DTMF_VD2_SIG[6]  && (ambe[7] & DTMF_VD2_MASK[7])   == DTMF_VD2_SIG[7] &&
		(ambe[8] & DTMF_VD2_MASK[8])   == DTMF_VD2_SIG[8]  && (ambe[9] & DTMF_VD2_MASK[9])   == DTMF_VD2_SIG[9] &&
		(ambe[10] & DTMF_VD2_MASK[10]) == DTMF_VD2_SIG[10] && (ambe[11] & DTMF_VD2_MASK[11]) == DTMF_VD2_SIG[11] &&
		(ambe[12] & DTMF_VD2_MASK[12]) == DTMF_VD2_SIG[12]) {
		unsigned char sym0  = ambe[0]  & DTMF_VD2_SYM_MASK[0];
		unsigned char sym1  = ambe[1]  & DTMF_VD2_SYM_MASK[1];
		unsigned char sym2  = ambe[2]  & DTMF_VD2_SYM_MASK[2];
		unsigned char sym3  = ambe[3]  & DTMF_VD2_SYM_MASK[3];
		unsigned char sym4  = ambe[4]  & DTMF_VD2_SYM_MASK[4];
		unsigned char sym5  = ambe[5]  & DTMF_VD2_SYM_MASK[5];
		unsigned char sym6  = ambe[8]  & DTMF_VD2_SYM_MASK[6];
		unsigned char sym7  = ambe[9]  & DTMF_VD2_SYM_MASK[7];
		unsigned char sym8  = ambe[10] & DTMF_VD2_SYM_MASK[8];
		unsigned char sym9  = ambe[11] & DTMF_VD2_SYM_MASK[9];
		unsigned char sym10 = ambe[12] & DTMF_VD2_SYM_MASK[10];

		char c = ' ';
		if (sym0 == DTMF_VD2_SYM0[0] && sym1 == DTMF_VD2_SYM0[1] && sym2 == DTMF_VD2_SYM0[2] && sym3 == DTMF_VD2_SYM0[3] && sym4 == DTMF_VD2_SYM0[4] && sym5 == DTMF_VD2_SYM0[5] && sym6 == DTMF_VD2_SYM0[6] && sym7 == DTMF_VD2_SYM0[7] && sym8 == DTMF_VD2_SYM0[8] && sym9 == DTMF_VD2_SYM0[9] && sym10 == DTMF_VD2_SYM0[10])
			c = '0';
		else if (sym0 == DTMF_VD2_SYM1[0] && sym1 == DTMF_VD2_SYM1[1] && sym2 == DTMF_VD2_SYM1[2] && sym3 == DTMF_VD2_SYM1[3] && sym4 == DTMF_VD2_SYM1[4] && sym5 == DTMF_VD2_SYM1[5] && sym6 == DTMF_VD2_SYM1[6] && sym7 == DTMF_VD2_SYM1[7] && sym8 == DTMF_VD2_SYM1[8] && sym9 == DTMF_VD2_SYM1[9] && sym10 == DTMF_VD2_SYM1[10])
			c = '1';
		else if (sym0 == DTMF_VD2_SYM2[0] && sym1 == DTMF_VD2_SYM2[1] && sym2 == DTMF_VD2_SYM2[2] && sym3 == DTMF_VD2_SYM2[3] && sym4 == DTMF_VD2_SYM2[4] && sym5 == DTMF_VD2_SYM2[5] && sym6 == DTMF_VD2_SYM2[6] && sym7 == DTMF_VD2_SYM2[7] && sym8 == DTMF_VD2_SYM2[8] && sym9 == DTMF_VD2_SYM2[9] && sym10 == DTMF_VD2_SYM2[10])
			c = '2';
		else if (sym0 == DTMF_VD2_SYM3[0] && sym1 == DTMF_VD2_SYM3[1] && sym2 == DTMF_VD2_SYM3[2] && sym3 == DTMF_VD2_SYM3[3] && sym4 == DTMF_VD2_SYM3[4] && sym5 == DTMF_VD2_SYM3[5] && sym6 == DTMF_VD2_SYM3[6] && sym7 == DTMF_VD2_SYM3[7] && sym8 == DTMF_VD2_SYM3[8] && sym9 == DTMF_VD2_SYM3[9] && sym10 == DTMF_VD2_SYM3[10])
			c = '3';
		else if (sym0 == DTMF_VD2_SYM4[0] && sym1 == DTMF_VD2_SYM4[1] && sym2 == DTMF_VD2_SYM4[2] && sym3 == DTMF_VD2_SYM4[3] && sym4 == DTMF_VD2_SYM4[4] && sym5 == DTMF_VD2_SYM4[5] && sym6 == DTMF_VD2_SYM4[6] && sym7 == DTMF_VD2_SYM4[7] && sym8 == DTMF_VD2_SYM4[8] && sym9 == DTMF_VD2_SYM4[9] && sym10 == DTMF_VD2_SYM4[10])
			c = '4';
		else if (sym0 == DTMF_VD2_SYM5[0] && sym1 == DTMF_VD2_SYM5[1] && sym2 == DTMF_VD2_SYM5[2] && sym3 == DTMF_VD2_SYM5[3] && sym4 == DTMF_VD2_SYM5[4] && sym5 == DTMF_VD2_SYM5[5] && sym6 == DTMF_VD2_SYM5[6] && sym7 == DTMF_VD2_SYM5[7] && sym8 == DTMF_VD2_SYM5[8] && sym9 == DTMF_VD2_SYM5[9] && sym10 == DTMF_VD2_SYM5[10])
			c = '5';
		else if (sym0 == DTMF_VD2_SYM6[0] && sym1 == DTMF_VD2_SYM6[1] && sym2 == DTMF_VD2_SYM6[2] && sym3 == DTMF_VD2_SYM6[3] && sym4 == DTMF_VD2_SYM6[4] && sym5 == DTMF_VD2_SYM6[5] && sym6 == DTMF_VD2_SYM6[6] && sym7 == DTMF_VD2_SYM6[7] && sym8 == DTMF_VD2_SYM6[8] && sym9 == DTMF_VD2_SYM6[9] && sym10 == DTMF_VD2_SYM6[10])
			c = '6';
		else if (sym0 == DTMF_VD2_SYM7[0] && sym1 == DTMF_VD2_SYM7[1] && sym2 == DTMF_VD2_SYM7[2] && sym3 == DTMF_VD2_SYM7[3] && sym4 == DTMF_VD2_SYM7[4] && sym5 == DTMF_VD2_SYM7[5] && sym6 == DTMF_VD2_SYM7[6] && sym7 == DTMF_VD2_SYM7[7] && sym8 == DTMF_VD2_SYM7[8] && sym9 == DTMF_VD2_SYM7[9] && sym10 == DTMF_VD2_SYM7[10])
			c = '7';
		else if (sym0 == DTMF_VD2_SYM8[0] && sym1 == DTMF_VD2_SYM8[1] && sym2 == DTMF_VD2_SYM8[2] && sym3 == DTMF_VD2_SYM8[3] && sym4 == DTMF_VD2_SYM8[4] && sym5 == DTMF_VD2_SYM8[5] && sym6 == DTMF_VD2_SYM8[6] && sym7 == DTMF_VD2_SYM8[7] && sym8 == DTMF_VD2_SYM8[8] && sym9 == DTMF_VD2_SYM8[9] && sym10 == DTMF_VD2_SYM8[10])
			c = '8';
		else if (sym0 == DTMF_VD2_SYM9[0] && sym1 == DTMF_VD2_SYM9[1] && sym2 == DTMF_VD2_SYM9[2] && sym3 == DTMF_VD2_SYM9[3] && sym4 == DTMF_VD2_SYM9[4] && sym5 == DTMF_VD2_SYM9[5] && sym6 == DTMF_VD2_SYM9[6] && sym7 == DTMF_VD2_SYM9[7] && sym8 == DTMF_VD2_SYM9[8] && sym9 == DTMF_VD2_SYM9[9] && sym10 == DTMF_VD2_SYM9[10])
			c = '9';
		else if (sym0 == DTMF_VD2_SYMA[0] && sym1 == DTMF_VD2_SYMA[1] && sym2 == DTMF_VD2_SYMA[2] && sym3 == DTMF_VD2_SYMA[3] && sym4 == DTMF_VD2_SYMA[4] && sym5 == DTMF_VD2_SYMA[5] && sym6 == DTMF_VD2_SYMA[6] && sym7 == DTMF_VD2_SYMA[7] && sym8 == DTMF_VD2_SYMA[8] && sym9 == DTMF_VD2_SYMA[9] && sym10 == DTMF_VD2_SYMA[10])
			c = 'A';
		else if (sym0 == DTMF_VD2_SYMB[0] && sym1 == DTMF_VD2_SYMB[1] && sym2 == DTMF_VD2_SYMB[2] && sym3 == DTMF_VD2_SYMB[3] && sym4 == DTMF_VD2_SYMB[4] && sym5 == DTMF_VD2_SYMB[5] && sym6 == DTMF_VD2_SYMB[6] && sym7 == DTMF_VD2_SYMB[7] && sym8 == DTMF_VD2_SYMB[8] && sym9 == DTMF_VD2_SYMB[9] && sym10 == DTMF_VD2_SYMB[10])
			c = 'B';
		else if (sym0 == DTMF_VD2_SYMC[0] && sym1 == DTMF_VD2_SYMC[1] && sym2 == DTMF_VD2_SYMC[2] && sym3 == DTMF_VD2_SYMC[3] && sym4 == DTMF_VD2_SYMC[4] && sym5 == DTMF_VD2_SYMC[5] && sym6 == DTMF_VD2_SYMC[6] && sym7 == DTMF_VD2_SYMC[7] && sym8 == DTMF_VD2_SYMC[8] && sym9 == DTMF_VD2_SYMC[9] && sym10 == DTMF_VD2_SYMC[10])
			c = 'C';
		else if (sym0 == DTMF_VD2_SYMD[0] && sym1 == DTMF_VD2_SYMD[1] && sym2 == DTMF_VD2_SYMD[2] && sym3 == DTMF_VD2_SYMD[3] && sym4 == DTMF_VD2_SYMD[4] && sym5 == DTMF_VD2_SYMD[5] && sym6 == DTMF_VD2_SYMD[6] && sym7 == DTMF_VD2_SYMD[7] && sym8 == DTMF_VD2_SYMD[8] && sym9 == DTMF_VD2_SYMD[9] && sym10 == DTMF_VD2_SYMD[10])
			c = 'D';
		else if (sym0 == DTMF_VD2_SYMS[0] && sym1 == DTMF_VD2_SYMS[1] && sym2 == DTMF_VD2_SYMS[2] && sym3 == DTMF_VD2_SYMS[3] && sym4 == DTMF_VD2_SYMS[4] && sym5 == DTMF_VD2_SYMS[5] && sym6 == DTMF_VD2_SYMS[6] && sym7 == DTMF_VD2_SYMS[7] && sym8 == DTMF_VD2_SYMS[8] && sym9 == DTMF_VD2_SYMS[9] && sym10 == DTMF_VD2_SYMS[10])
			c = '*';
		else if (sym0 == DTMF_VD2_SYMH[0] && sym1 == DTMF_VD2_SYMH[1] && sym2 == DTMF_VD2_SYMH[2] && sym3 == DTMF_VD2_SYMH[3] && sym4 == DTMF_VD2_SYMH[4] && sym5 == DTMF_VD2_SYMH[5] && sym6 == DTMF_VD2_SYMH[6] && sym7 == DTMF_VD2_SYMH[7] && sym8 == DTMF_VD2_SYMH[8] && sym9 == DTMF_VD2_SYMH[9] && sym10 == DTMF_VD2_SYMH[10])
			c = '#';

		// Blank out the DTMF tones.
		if (c != ' ')
			::memcpy(ambe, VD2_SILENCE, 13U);

		if (c == m_lastChar) {
			m_pressCount++;
		} else {
			m_lastChar = c;
			m_pressCount = 0U;
		}

		if (c != ' ' && !m_pressed && m_pressCount >= 3U) {
			m_data += c;
			m_releaseCount = 0U;
			m_pressed = true;
		}

		return validate();
	} else {
		// If it is not a DTMF Code
		if ((end || m_releaseCount >= 100U) && m_data.length() > 0U) {
			m_command = m_data;
			m_data.clear();
			m_releaseCount = 0U;
		}

		m_pressed = false;
		m_releaseCount++;
		m_pressCount = 0U;
		m_lastChar = ' ';

		return validate();
	}
}

WX_STATUS CDTMF::validate() const
{
	if (m_command.empty())
		return WX_STATUS::NONE;

	size_t length = m_command.length();
	char first    = m_command.at(0U);

	if (length == 1U && first == '#') {
		return WX_STATUS::DISCONNECT;
	} else if (length == 3U && first == 'A') {
		for (unsigned int i = 1U; i < 3U; i++) {
			char c = m_command.at(i);
			if (c < '0' || c > '9')
				return WX_STATUS::NONE;
		}

		return WX_STATUS::CONNECT_FCS;
	} else if (length == 4U && first == 'A') {
		for (unsigned int i = 1U; i < 4U; i++) {
			char c = m_command.at(i);
			if (c < '0' || c > '9')
				return WX_STATUS::NONE;
		}

		return WX_STATUS::CONNECT_FCS;
	} else if (length == 6U && first == '#') {
		for (unsigned int i = 1U; i < 6U; i++) {
			char c = m_command.at(i);
			if (c < '0' || c > '9')
				return WX_STATUS::NONE;
		}

		if (m_command == "#99999")
			return WX_STATUS::DISCONNECT;

		return WX_STATUS::CONNECT_YSF;
	}

	return WX_STATUS::NONE;
}

std::string CDTMF::getReflector()
{
	std::string command = m_command;
	reset();

	if (command.empty())
		return "";

	return command.substr(1U);
}

void CDTMF::reset()
{
	m_data.clear();
	m_command.clear();
	m_pressed = false;
	m_pressCount = 0U;
	m_releaseCount = 0U;
	m_lastChar = ' ';
}
