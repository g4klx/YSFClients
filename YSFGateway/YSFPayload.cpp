/*
*	Copyright (C) 2016,2025 Jonathan Naylor, G4KLX
*	Copyright (C) 2016 Mathias Weyland, HB9FRV
*
*	This program is free software; you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation; version 2 of the License.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*/

#include "YSFConvolution.h"
#include "YSFPayload.h"
#include "YSFDefines.h"
#include "Utils.h"
#include "CRC.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdint>

const unsigned int INTERLEAVE_TABLE_9_20[] = {
        0U, 40U,  80U, 120U, 160U, 200U, 240U, 280U, 320U, 
        2U, 42U,  82U, 122U, 162U, 202U, 242U, 282U, 322U,
        4U, 44U,  84U, 124U, 164U, 204U, 244U, 284U, 324U,
        6U, 46U,  86U, 126U, 166U, 206U, 246U, 286U, 326U,
        8U, 48U,  88U, 128U, 168U, 208U, 248U, 288U, 328U,
       10U, 50U,  90U, 130U, 170U, 210U, 250U, 290U, 330U,
       12U, 52U,  92U, 132U, 172U, 212U, 252U, 292U, 332U,
       14U, 54U,  94U, 134U, 174U, 214U, 254U, 294U, 334U,
       16U, 56U,  96U, 136U, 176U, 216U, 256U, 296U, 336U,
       18U, 58U,  98U, 138U, 178U, 218U, 258U, 298U, 338U,
       20U, 60U, 100U, 140U, 180U, 220U, 260U, 300U, 340U,
       22U, 62U, 102U, 142U, 182U, 222U, 262U, 302U, 342U,
       24U, 64U, 104U, 144U, 184U, 224U, 264U, 304U, 344U,
       26U, 66U, 106U, 146U, 186U, 226U, 266U, 306U, 346U,
       28U, 68U, 108U, 148U, 188U, 228U, 268U, 308U, 348U,
       30U, 70U, 110U, 150U, 190U, 230U, 270U, 310U, 350U,
       32U, 72U, 112U, 152U, 192U, 232U, 272U, 312U, 352U,
       34U, 74U, 114U, 154U, 194U, 234U, 274U, 314U, 354U,
       36U, 76U, 116U, 156U, 196U, 236U, 276U, 316U, 356U,
       38U, 78U, 118U, 158U, 198U, 238U, 278U, 318U, 358U};

const unsigned int INTERLEAVE_TABLE_5_20[] = {
	0U, 40U,  80U, 120U, 160U,
	2U, 42U,  82U, 122U, 162U,
	4U, 44U,  84U, 124U, 164U,
	6U, 46U,  86U, 126U, 166U,
	8U, 48U,  88U, 128U, 168U,
	10U, 50U,  90U, 130U, 170U,
	12U, 52U,  92U, 132U, 172U,
	14U, 54U,  94U, 134U, 174U,
	16U, 56U,  96U, 136U, 176U,
	18U, 58U,  98U, 138U, 178U,
	20U, 60U, 100U, 140U, 180U,
	22U, 62U, 102U, 142U, 182U,
	24U, 64U, 104U, 144U, 184U,
	26U, 66U, 106U, 146U, 186U,
	28U, 68U, 108U, 148U, 188U,
	30U, 70U, 110U, 150U, 190U,
	32U, 72U, 112U, 152U, 192U,
	34U, 74U, 114U, 154U, 194U,
	36U, 76U, 116U, 156U, 196U,
	38U, 78U, 118U, 158U, 198U};

const unsigned char WHITENING_DATA[] = {0x93U, 0xD7U, 0x51U, 0x21U, 0x9CU, 0x2FU, 0x6CU, 0xD0U, 0xEFU, 0x0FU,
										0xF8U, 0x3DU, 0xF1U, 0x73U, 0x20U, 0x94U, 0xEDU, 0x1EU, 0x7CU, 0xD8U};

const unsigned char BIT_MASK_TABLE[] = {0x80U, 0x40U, 0x20U, 0x10U, 0x08U, 0x04U, 0x02U, 0x01U};

#define WRITE_BIT1(p,i,b) p[(i)>>3] = (b) ? (p[(i)>>3] | BIT_MASK_TABLE[(i)&7]) : (p[(i)>>3] & ~BIT_MASK_TABLE[(i)&7])
#define READ_BIT1(p,i)    (p[(i)>>3] & BIT_MASK_TABLE[(i)&7])

CYSFPayload::CYSFPayload()
{
}

CYSFPayload::~CYSFPayload()
{
}

bool CYSFPayload::readVDMode1Data(const unsigned char* data, unsigned char* dt)
{
	assert(data != nullptr);
	assert(dt != nullptr);

	data += YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES;

	unsigned char dch[45U];

	const unsigned char* p1 = data;
	unsigned char* p2 = dch;
	for (unsigned int i = 0U; i < 5U; i++) {
		::memcpy(p2, p1, 9U);
		p1 += 18U; p2 += 9U;
	}

	CYSFConvolution conv;
	conv.start();

	for (unsigned int i = 0U; i < 180U; i++) {
		unsigned int n = INTERLEAVE_TABLE_9_20[i];
		uint8_t s0 = READ_BIT1(dch, n) ? 1U : 0U;

		n++;
		uint8_t s1 = READ_BIT1(dch, n) ? 1U : 0U;

		conv.decode(s0, s1);
	}

	unsigned char output[23U];
	conv.chainback(output, 176U);

	bool ret = CCRC::checkCCITT16(output, 22U);
	if (ret) {
		for (unsigned int i = 0U; i < 20U; i++)
			output[i] ^= WHITENING_DATA[i];

		CUtils::dump(1U, "V/D Mode 1 Data", output, 20U);

		::memcpy(dt, output, 20U);
	}

	return ret;
}

bool CYSFPayload::readVDMode2Data(const unsigned char* data, unsigned char* dt)
{
	assert(data != nullptr);
	assert(dt != nullptr);

	data += YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES;

	unsigned char dch[25U];

	const unsigned char* p1 = data;
	unsigned char* p2 = dch;
	for (unsigned int i = 0U; i < 5U; i++) {
		::memcpy(p2, p1, 5U);
		p1 += 18U; p2 += 5U;
	}

	CYSFConvolution conv;
	conv.start();

	for (unsigned int i = 0U; i < 100U; i++) {
		unsigned int n = INTERLEAVE_TABLE_5_20[i];
		uint8_t s0 = READ_BIT1(dch, n) ? 1U : 0U;

		n++;
		uint8_t s1 = READ_BIT1(dch, n) ? 1U : 0U;

		conv.decode(s0, s1);
	}

	unsigned char output[13U];
	conv.chainback(output, 96U);

	bool ret = CCRC::checkCCITT16(output, 12U);
	if (ret) {
		for (unsigned int i = 0U; i < 10U; i++)
			output[i] ^= WHITENING_DATA[i];

		CUtils::dump(1U, "V/D Mode 2 Data", output, YSF_CALLSIGN_LENGTH);

		::memcpy(dt, output, YSF_CALLSIGN_LENGTH);
	}

	return ret;
}

bool CYSFPayload::readDataFRModeData1(const unsigned char* data, unsigned char* dt)
{
	assert(data != nullptr);
	assert(dt != nullptr);

	::memset(dt, ' ', 20U);

	data += YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES;

	unsigned char dch[45U];

	const unsigned char* p1 = data;
	unsigned char* p2 = dch;
	for (unsigned int i = 0U; i < 5U; i++) {
		::memcpy(p2, p1, 9U);
		p1 += 18U; p2 += 9U;
	}

	CYSFConvolution conv;
	conv.start();

	for (unsigned int i = 0U; i < 180U; i++) {
		unsigned int n = INTERLEAVE_TABLE_9_20[i];
		uint8_t s0 = READ_BIT1(dch, n) ? 1U : 0U;

		n++;
		uint8_t s1 = READ_BIT1(dch, n) ? 1U : 0U;

		conv.decode(s0, s1);
	}

	unsigned char output[23U];
	conv.chainback(output, 176U);

	bool ret = CCRC::checkCCITT16(output, 22U);
	if (ret) {
		for (unsigned int i = 0U; i < 20U; i++)
			output[i] ^= WHITENING_DATA[i];

		CUtils::dump(1U, "FR Mode Data 1", output, 20U);

		::memcpy(dt, output, 20U);
	}

	return ret;
}

bool CYSFPayload::readDataFRModeData2(const unsigned char* data, unsigned char* dt)
{
	assert(data != nullptr);
	assert(dt != nullptr);

	::memset(dt, ' ', 20U);

	data += YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES;

	unsigned char dch[45U];

	const unsigned char* p1 = data + 9U;
	unsigned char* p2 = dch;
	for (unsigned int i = 0U; i < 5U; i++) {
		::memcpy(p2, p1, 9U);
		p1 += 18U; p2 += 9U;
	}

	CYSFConvolution conv;
	conv.start();

	for (unsigned int i = 0U; i < 180U; i++) {
		unsigned int n = INTERLEAVE_TABLE_9_20[i];
		uint8_t s0 = READ_BIT1(dch, n) ? 1U : 0U;

		n++;
		uint8_t s1 = READ_BIT1(dch, n) ? 1U : 0U;

		conv.decode(s0, s1);
	}

	unsigned char output[23U];
	conv.chainback(output, 176U);

	bool ret = CCRC::checkCCITT16(output, 22U);
	if (ret) {
		for (unsigned int i = 0U; i < 20U; i++)
			output[i] ^= WHITENING_DATA[i];

		CUtils::dump(1U, "FR Mode Data 2", output, 20U);

		::memcpy(dt, output, 20U);
	}

	return ret;
}

void CYSFPayload::writeDataFRModeData1(const unsigned char* dt, unsigned char* data)
{
	assert(dt != nullptr);
	assert(data != nullptr);

	data += YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES;

	unsigned char output[25U];
	for (unsigned int i = 0U; i < 20U; i++)
		output[i] = dt[i] ^ WHITENING_DATA[i];

	CCRC::addCCITT16(output, 22U);
	output[22U] = 0x00U;

	unsigned char convolved[45U];

	CYSFConvolution conv;
	conv.encode(output, convolved, 180U);

	unsigned char bytes[45U] = { 0x00U };
	unsigned int j = 0U;
	for (unsigned int i = 0U; i < 180U; i++) {
		unsigned int n = INTERLEAVE_TABLE_9_20[i];

		bool s0 = READ_BIT1(convolved, j) != 0U;
		j++;

		bool s1 = READ_BIT1(convolved, j) != 0U;
		j++;

		WRITE_BIT1(bytes, n, s0);

		n++;
		WRITE_BIT1(bytes, n, s1);
	}

	unsigned char* p1 = data;
	unsigned char* p2 = bytes;
	for (unsigned int i = 0U; i < 5U; i++) {
		::memcpy(p1, p2, 9U);
		p1 += 18U; p2 += 9U;
	}
}

void CYSFPayload::writeDataFRModeData2(const unsigned char* dt, unsigned char* data)
{
	assert(dt != nullptr);
	assert(data != nullptr);

	data += YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES;

	unsigned char output[25U];
	for (unsigned int i = 0U; i < 20U; i++)
		output[i] = dt[i] ^ WHITENING_DATA[i];

	CCRC::addCCITT16(output, 22U);
	output[22U] = 0x00U;

	unsigned char convolved[45U];

	CYSFConvolution conv;
	conv.encode(output, convolved, 180U);

	unsigned char bytes[45U] = { 0x00U };
	unsigned int j = 0U;
	for (unsigned int i = 0U; i < 180U; i++) {
		unsigned int n = INTERLEAVE_TABLE_9_20[i];

		bool s0 = READ_BIT1(convolved, j) != 0U;
		j++;

		bool s1 = READ_BIT1(convolved, j) != 0U;
		j++;

		WRITE_BIT1(bytes, n, s0);

		n++;
		WRITE_BIT1(bytes, n, s1);
	}

	unsigned char* p1 = data + 9U;
	unsigned char* p2 = bytes;
	for (unsigned int i = 0U; i < 5U; i++) {
		::memcpy(p1, p2, 9U);
		p1 += 18U; p2 += 9U;
	}
}
